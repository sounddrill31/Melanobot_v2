/**
 * \file
 *
 * \author Mattia Basaglia
 *
 * \copyright Copyright 2016 Mattia Basaglia
 *
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

#include "pages.hpp"
#include "config.hpp"
#include "melanobot/melanobot.hpp"
#include "formatter_html.hpp"

using namespace httpony::quick_xml;
using namespace httpony::quick_xml::html;

namespace web {

static melanobot::Melanobot& bot()
{
    return melanobot::Melanobot::instance();
}


std::shared_ptr<BlockElement> page_link(
    const Request& request,
    const httpony::Path& path,
    const Text& text)
{
    if ( WebPage::PathSuffix(request.uri.path).match_exactly(path) )
        return std::make_shared<Element>("span", text);
    else
        return std::make_shared<Link>(path.url_encoded(true), text);
}

static List connection_list(const Request& request, const httpony::Path& base_path)
{
    List connections;
    for ( const auto& conn : bot().connection_names() )
        connections.add_item(page_link(request, base_path/"connection"/conn, conn));
    return connections;
}

static List service_list(const Request& request, const httpony::Path& base_path)
{
    List services;
    for ( const auto& svc : bot().service_list() )
        services.add_item(page_link(
            request,
            base_path/"service"/std::to_string(uintptr_t(svc.get())),
            svc->name()));
    return services;
}

static void flatten_tree(
    const PropertyTree& tree,
    const std::string& prefix,
    httpony::quick_xml::html::Table& table)
{
    for ( const auto& prop : tree)
    {
        table.add_data_row(Text(prop.first), Text(prop.second.data()));
        flatten_tree(prop.second, prefix + prop.first + '.', table);
    }
}

static httpony::quick_xml::BlockElement status(network::Connection::Status status)
{
    std::string status_name = "Disconnected";
    if ( status > network::Connection::CHECKING )
        status_name = "Connected";
    else if ( status >= network::Connection::CONNECTING )
        status_name = "Connecting";

    return BlockElement{
        "span",
        Attribute("class", "status_" + melanolib::string::strtolower(status_name)),
        Text(status_name)
    };
}

class StatusPage::SubPage
{
public:
    using BlockElement = httpony::quick_xml::BlockElement;
    using PathSuffix = WebPage::PathSuffix;

    SubPage(std::string name, httpony::Path path)
        : _name(std::move(name)), _path(std::move(path))
    {}

    virtual ~SubPage(){}

    virtual bool match_path(const PathSuffix& path) const
    {
        return path.match_prefix(_path);
    }

    const std::string& name() const
    {
        return _name;
    }

    const httpony::Path& path() const
    {
        return _path;
    }

    virtual void render(
        Request& request,
        const PathSuffix& path,
        BlockElement& parent,
        const httpony::Path& link_base_path
    ) const = 0;

    virtual bool has_submenu() const
    {
        return false;
    }

    virtual List submenu(const Request& request, const httpony::Path& link_base_path) const
    {
        return List();
    }

private:
    std::string _name;
    httpony::Path _path;
};

class Home : public StatusPage::SubPage
{
public:
    Home() : SubPage("Home", "") {}

    bool match_path(const PathSuffix& path) const
    {
        return path.empty();
    }

    void render(
        Request& request,
        const PathSuffix& path,
        BlockElement& parent,
        const httpony::Path& link_base_path
    ) const override
    {
        parent.append(Element{"h1", Text{PROJECT_NAME}});
        parent.append(Element{"h2", Text{"Connections"}});
        parent.append(connection_list(request, link_base_path));
        parent.append(Element{"h2", Text{"Services"}});
        parent.append(service_list(request, link_base_path));

    }
};

class Connections : public StatusPage::SubPage
{
public:
    Connections() : SubPage("Connections", "connection") {}

    void render(
        Request& request,
        const PathSuffix& path,
        BlockElement& parent,
        const httpony::Path& link_base_path
    ) const override
    {
        if ( path.size() == 1 )
        {
            parent.append(Element{"h1", Text{"Connections"}});
            parent.append(connection_list(request, link_base_path));
            return;
        }

        if ( path.size() != 2 )
            throw HttpError(StatusCode::NotFound);

        network::Connection* conn = bot().connection(path[1]);
        if ( !conn )
            throw HttpError(StatusCode::NotFound);

        parent.append(Element{"h1", Text{path[1]}});

        Table table;
        table.add_header_row(Text{"Property"}, Text{"Value"});
        table.add_data_row(Text("Protocol"), Text(conn->protocol()));
        table.add_data_row(Text("Status"), status(conn->status()));
        table.add_data_row(Text("Name"), Text(conn->name()));
        table.add_data_row(Text("Config Name"), Text(conn->config_name()));
        table.add_data_row(Text("Formatter"), Text(conn->formatter()->name()));
        table.add_data_row(Text("Server"), Text(conn->server().name()));

        auto pretty = conn->pretty_properties();
        if ( !pretty.empty() )
        {
            table.add_row(Element("th", Attribute("colspan", "2"), Text("Formatting")));
            FormatterHtml formatter;
            for ( const auto& prop : pretty )
                table.add_data_row(Text(prop.first), Text(prop.second.encode(formatter)));
        }

        auto internal = conn->properties().copy();
        if ( !internal.empty() )
        {
            table.add_row(Element("th", Attribute("colspan", "2"), Text("Internal")));
            flatten_tree(internal, "", table);
        }

        parent.append(table);
    }

    virtual bool has_submenu() const override
    {
        return true;
    }

    virtual List submenu(const Request& request, const httpony::Path& link_base_path) const override
    {
        return connection_list(request, link_base_path);
    }
};

class Services : public StatusPage::SubPage
{
public:
    Services() : SubPage("Services", "service") {}

    void render(
        Request& request,
        const PathSuffix& path,
        BlockElement& parent,
        const httpony::Path& link_base_path
    ) const override
    {
        if ( path.size() == 1 )
        {
            parent.append(Element{"h1", Text{"Services"}});
            parent.append(service_list(request, link_base_path));
            return;
        }

        if ( path.size() != 2 )
            throw HttpError(StatusCode::NotFound);

        AsyncService* service = nullptr;
        for ( const auto& svc : bot().service_list() )
            if ( std::to_string(uintptr_t(svc.get())) == path[1] )
            {
                service = svc.get();
                break;
            }
        if ( !service )
            throw HttpError(StatusCode::NotFound);

        parent.append(Element{"h1", Text{service->name()}});

        Table table;
        table.add_header_row(Text{"Property"}, Text{"Value"});
        table.add_data_row(Text("Status"), status(
            service->running() ?
                network::Connection::CONNECTED :
                network::Connection::DISCONNECTED
        ));
        table.add_data_row(Text("Name"), Text(service->name()));

        parent.append(table);
    }

    virtual bool has_submenu() const override
    {
        return true;
    }

    virtual List submenu(const Request& request, const httpony::Path& link_base_path) const override
    {
        return service_list(request, link_base_path);
    }
};

StatusPage::StatusPage(const Settings& settings)
{
    uri = read_uri(settings, "");
    css_file = settings.get("css", css_file);
    sub_pages.push_back(melanolib::New<Home>());
    sub_pages.push_back(melanolib::New<Connections>());
    sub_pages.push_back(melanolib::New<Services>());
}

StatusPage::~StatusPage() = default;

Response StatusPage::respond(Request& request, const PathSuffix& path, const HttpServer& sv) const
{
    auto local_path = path.left_stripped(uri.size());
    HtmlDocument html("Bot status");
    httpony::Path base_path = local_path.strip_path_suffix(request.uri.path).to_path();

    if ( !css_file.empty() )
    {
        html.head().append(Element("link", Attributes{
            {"rel", "stylesheet"}, {"type", "text/css"}, {"href", css_file}
        }));
    }

    List nav;
    SubPage* current_page = nullptr;
    for ( const auto& page : sub_pages )
    {
        if ( page->match_path(local_path) )
            current_page = page.get();

        nav.add_item(page_link(request, base_path + page->path(), page->name()));
    }
    if ( !current_page )
        throw HttpError(StatusCode::NotFound);

    html.body().append(Element("nav", Attribute("class", "menu"), nav));
    if ( current_page->has_submenu() )
        html.body().append(Element("nav", Attribute("class", "submenu"),
            current_page->submenu(request, base_path)));
    BlockElement contents("div", Attribute("class", "contents"));
    current_page->render(request, local_path, contents, base_path);
    html.body().append(contents);

    Response response("text/html", StatusCode::OK, request.protocol);
    html.print(response.body, true);
    return response;
}
} // namespace web