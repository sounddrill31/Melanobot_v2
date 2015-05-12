/**
 * \file
 * \author Mattia Basaglia
 * \copyright Copyright 2015 Mattia Basaglia
 * \license
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <atomic>
#include <cstdint>

#include "string/logger.hpp"
#include "string/string_functions.hpp"
#include "user/auth_system.hpp"
#include "user/user_counter.hpp"
#include "user/user_manager.hpp"
#include "network.hpp"
#include "concurrency/locked_properties.hpp"

class Melanobot;

namespace network {

/**
 * \brief A command to send to a connection
 */
struct Command
{
    std::string                 command;        ///< Command name
    std::vector<std::string>    parameters;     ///< Optional parameters
    int                         priority;       ///< Priority, higher = handled sooner
    Time                        timein;         ///< Time of creation
    Time                        timeout;        ///< Time it becomes obsolete

    Command ( std::string         command = {},
        std::vector<std::string>  parameters = {},
        int                       priority = 0,
        Time                      timeout = Time::max() )
    :   command(std::move(command)),
        parameters(std::move(parameters)),
        priority(priority),
        timein(Clock::now()),
        timeout(std::move(timeout))
    {}

    Command ( std::string         command,
        std::vector<std::string>  parameters,
        int                             priority,
        const Duration&                 duration )
    :   command(std::move(command)),
        parameters(std::move(parameters)),
        priority(priority),
        timein(Clock::now()),
        timeout(Clock::now()+duration)
    {}

    Command(const Command&) = default;
    Command(Command&&) noexcept = default;
    Command& operator=(const Command&) = default;
    Command& operator=(Command&&) /*noexcept(std::is_nothrow_move_assignable<std::string>::value)*/ = default;

    bool operator< ( const Command& other ) const
    {
        return priority < other.priority ||
            ( priority == other.priority && timein > other.timein );
    }
};

/**
 * \brief A message originating from a connection
 */
struct Message
{
    /**
     * \brief Type of message
     */
    enum Type
    {
        /**
         * \brief Some unknown message, most likely protocol-specific stuff
         */
        UNKNOWN,
        /**
         * \brief Simple chat message
         *
         * These messages should give meaningful values to:
         *      * \c message
         *      * \c from
         *      * \c direct
         *      * \c channels
         */
        CHAT,
        /**
         * \brief Similar to \c CHAT, but used for actions/roleplay
         *
         * These messages should give meaningful values to:
         *      * \c message
         *      * \c from
         *      * \c channels
         */
        ACTION,
        /**
         * \brief User joined the connection/a channel
         *
         * These messages should give meaningful values to:
         *      * \c from
         *      * \c channels
         */
        JOIN,
        /**
         * \brief User parted or quit
         *
         * These messages should give meaningful values to:
         *      * \c message
         *      * \c from
         *      * \c channels
         */
        PART,
        /**
         * \brief User has been kicked
         *
         * These messages should give meaningful values to:
         *      * \c message
         *      * \c from
         *      * \c victim
         *      * \c channels
         */
        KICK,
        /**
         * \brief User changed name
         *
         * These messages should give meaningful values to:
         *      * \c from
         */
        RENAME,
        /**
         * \brief Server error
         *
         * These messages should give meaningful values to:
         *      * \c message
         */
        ERROR,
        /**
         * \brief Connection activated
         */
        CONNECTED,
        /**
         * \brief Connection deactivated
         */
        DISCONNECTED,
    };
// origin
    class Connection*        source {nullptr};     ///< Connection originating this message
// reply
    class Connection*        destination {nullptr};///< Connection which should receive replies
// low level properties
    std::string              raw;     ///< Raw contents
    std::string              command; ///< Protocol command name
    std::vector<std::string> params;  ///< Tokenized parameters
// high level properties (all optional)
    Type                     type{UNKNOWN};///< Message type
    std::string              message;      ///< Message contents
    std::vector<std::string> channels;     ///< Channels affected by the message
    bool                     direct{false};///< Message has been addessed to the bot directly
    user::User               from;         ///< User who created this message
    user::User               victim;       ///< User victim of this command

    /**
     * \brief Set \c source and \c destination and sends to the given bot
     */
    void send(Connection* from, Melanobot* to);

    /**
     * \brief Turns into a CONNECTED message
     */
    Message& connected()
    {
        type = CONNECTED;
        return *this;
    }
    /**
     * \brief Turns into a DISCONNECTED message
     */
    Message& disconnected()
    {
        type = DISCONNECTED;
        return *this;
    }
};

/**
 * \brief A message given to a connection
 *
 * This is similar to \c Command but at a higher level
 * (doesn't require knowledge of the protocol used by the connection)
 */
struct OutputMessage
{
    OutputMessage(string::FormattedString message,
                  bool action = false,
                  std::string target = {},
                  int priority = 0,
                  string::FormattedString  from = {},
                  string::FormattedString  prefix = {},
                  Time  timeout = Clock::time_point::max()
    ) : target(std::move(target)), message(std::move(message)), priority(priority),
        from(std::move(from)), prefix(std::move(prefix)),
        action(action), timeout(std::move(timeout))
    {}

    OutputMessage(const OutputMessage&) = default;
    OutputMessage(OutputMessage&&) noexcept = default;
    OutputMessage& operator=(const OutputMessage&) = default;
    OutputMessage& operator=(OutputMessage&&) /*noexcept(std::is_nothrow_move_assignable<std::string>::value)*/ = default;


    /**
     * \brief Channel or user id to which the message shall be delivered to
     */
    std::string target;
    /**
     * \brief Message contents
     */
    string::FormattedString message;
    /**
     * \brief Priority, higher = handled sooner
     */
    int priority = 0;
    /**
     * \brief If not empty, the bot will make it look like the message comes from this user
     */
    string::FormattedString from;
    /**
     * \brief Prefix to prepend to the message
     */
    string::FormattedString prefix;
    /**
     * \brief Whether the message is an action
     */
    bool action = false;
    /**
     * \brief Time at which this message becomes obsolete
     */
    Time timeout;
};

/**
 * \brief Abstract service connection
 *
 * Instances of this class will be created by \c Melanobot from the
 * configuration and will send \c Message objects and receive either
 * \c Command or \c OutputMessage objects.
 *
 * To create your own class, inherith this one and override the pure virtual
 * methods. Then use Melanomodule::register_connection() to register
 * the class to the \c ConnectionFactory.
 *
 * It is recommended that you add a static method called \c create in the
 * derived class with the following signature:
 * \code{.cpp}
 *      YourClass* create(Melanobot* bot, const Settings& settings);
 * \endcode
 * to be used with Melanomodule::register_connection().
 *
 * You should also have a corresponding log type to use for the connection,
 * registered with Melanomodule::register_connection().
 */
class Connection
{
public:
    enum Status
    {
        DISCONNECTED,   ///< Connection is completely disconnected
        WAITING,        ///< Needs something before connecting
        CONNECTING,     ///< Needs some protocol action before becoming usable
        CHECKING,       ///< Connected, making sure the connection is alive
        CONNECTED,      ///< All set
    };
    using AtomicStatus = std::atomic<Status>;

    explicit Connection(std::string config_name) : config_name_(std::move(config_name)) {}
    Connection(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection& operator=(Connection&&) = delete;

    virtual ~Connection() {}

    /**
     * \brief Returns the server object to which this connection is connected to
     */
    virtual Server server() const = 0;

    /**
     * \brief Returns a 1-line description of the connection (including server info)
     */
    virtual std::string description() const = 0;

    /**
     * \brief Schedules a command for execution
     */
    virtual void command ( Command cmd ) = 0;

    /**
     * \brief Sends a message to the given channel
     */
    virtual void say ( const OutputMessage& message ) = 0;

    /**
     * \brief Returns the connection status
     * \todo is this ever called?
     */
    virtual Status status() const = 0;

    /**
     * \brief Protocol identifier
     */
    virtual std::string protocol() const = 0;

    /**
     * \brief Initialize the connection
     */
    virtual void connect() = 0;

    /**
     * \brief Close the connection
     */
    virtual void disconnect(const std::string& message = {}) = 0;

    /**
     * \brief Disconnect and connect again
     */
    virtual void reconnect(const std::string& quit_message = {}) = 0;

    /**
     * \brief Disconnect and stop all processing
     */
    virtual void stop() { disconnect(); }

    /**
     * \brief Start processing messages
     */
    virtual void start() { connect(); }

    /**
     * \brief Get the string formatter
     */
    virtual string::Formatter* formatter() const = 0;

    /**
     * \brief Decodes \c input with formatter()
     */
    string::FormattedString decode(const std::string& input) const
    {
        return formatter()->decode(input);
    }

    /**
     * \brief Decodes \c input with formatter() and re-encodes it using \c target_format
     */
    std::string encode_to(const std::string& input,
                          const string::Formatter& target_format) const
    {
        return decode(input).encode(target_format);
    }

    /**
     * \brief Whether a list of channels matches the mask
     * (Meaning depends on the specialized class)
     */
    virtual bool channel_mask(const std::vector<std::string>& channels,
                              const std::string& mask) const = 0;

    /**
     * \brief Get whether a user has the given authorization level
     */
    virtual bool user_auth(const std::string& local_id,
                           const std::string& auth_group) const = 0;
    /**
     * \brief Update the properties of a user by local_id
     */
    virtual void update_user(const std::string& local_id,
                             const Properties& properties) = 0;

    /**
     * \brief Returns a copy of the user object identified by \c local_id
     * \return A copy of the internal record identifying the user or
     *         an empty object if the user is not found.
     */
    virtual user::User get_user(const std::string& local_id) const = 0;

    /**
     * \brief Get a vector with the users in the given channel
     *        or all the users if empty
     */
    virtual std::vector<user::User> get_users( const std::string& channel_mask = "" ) const = 0;

    /**
     * \brief Adds a user identified by \c user to the group \c group
     * \param user  The user \c local_id, derived classes may perform some
     *              parsing to interpret it as a host or global_id
     * \param group Group name
     */
    virtual bool add_to_group(const std::string& user, const std::string& group) = 0;

    /**
     * \brief Removes a user identified by \c user to the group \c group
     * \param user  The user \c local_id, derived classes may perform some
     *              parsing to interpret it as a host or global_id
     * \param group Group name
     */
    virtual bool remove_from_group(const std::string& user, const std::string& group) = 0;

    /**
     * \brief Get a vector with the users in the given group (as set from the config)
     */
    virtual std::vector<user::User> users_in_group(const std::string& group) const = 0;

    /**
     * \brief Get a vector with the users in the given group (currently connected
     */
    virtual std::vector<user::User> real_users_in_group(const std::string& group) const = 0;

    /**
     * \brief Name of the service provided by this connection,
     *        as seen by the handled protocol
     */
    virtual std::string name() const = 0;

    /**
     * \brief Get connection properties
     */
    virtual LockedProperties properties() = 0;

    /**
     * \brief Counts the number of users in a channel (or the whole connection),
     *        as visible to the bot.
     */
    virtual user::UserCounter count_users(const std::string& channel = {} ) const = 0;

    /**
     * \brief Returns a list of properties used to message formatting
     * \note Returned properties should be formatted using the FormatterConfig
     */
    virtual Properties message_properties() const = 0;

    /**
     * \brief Returns the name of the connection as used in the config
     */
    const std::string& config_name() const { return config_name_; }

private:
    std::string config_name_;
};

/**
 * \brief Creates connections from settings
 */
class ConnectionFactory
{
public:
    using Contructor = std::function<std::unique_ptr<Connection>
        (Melanobot* bot, const Settings& settings, const std::string&name)>;

    /**
     * \brief Singleton instance
     */
    static ConnectionFactory& instance()
    {
        static ConnectionFactory singleton;
        return singleton;
    };

    /**
     * \brief Registers a connection type
     * \throws CriticalException If a protocol is defined twice
     */
    void register_connection ( const std::string& protocol_name,
                               const Contructor& function );

    /**
     * \brief Creates a connection from its settings
     */
    std::unique_ptr<Connection> create(Melanobot* bot,
                                       const Settings& settings,
                                       const std::string& name
                                      );

private:
    ConnectionFactory(){}
    ConnectionFactory(const ConnectionFactory&) = delete;
    ConnectionFactory(ConnectionFactory&&) = delete;
    ConnectionFactory& operator=(const ConnectionFactory&) = delete;
    ConnectionFactory& operator=(ConnectionFactory&&) = delete;

    std::unordered_map<std::string,Contructor> factory;
};


} // namespace network

#endif // CONNECTION_HPP
