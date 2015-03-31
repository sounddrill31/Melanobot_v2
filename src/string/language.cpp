/**
 * \file
 * \author Mattia Basaglia
 * \copyright Copyright 2015 Mattia Basaglia
 * \section License
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
#include "language.hpp"

namespace string {


std::string English::ordinal_suffix(int i) const
{

    if ( i <= 0 )
        return "";
    if ( i%100 < 11 || i%100 > 13 )
    {
        if ( i % 10 == 1 )
            return "st";
        if ( i % 10 == 2 )
            return "nd";
        if ( i % 10 == 3 )
            return "rd";
    }
    return "th";
}

std::string English::genitive(const std::string& noun) const
{
    return infl_genitive.inflect(noun);
}

std::string English::pronoun_to3rd(const std::string& sentence,
                                   const std::string& me,
                                   const std::string& you) const
{
    auto my = genitive(me);

    Inflector pronoun_swap ( {
        {"you",         you},
        {"your",        "its"},
        {"yours",       "its"},
        {"yourself",    "itself"},
        {"am",          "is"},
        {"I\'m",        me+" is"},
        {"I",           me},
        {"me",          me},
        {"my",          my},
        {"mine",        my},
        {"myself",      me},
    }, true );
    return pronoun_swap.inflect(sentence);

}

std::string English::imperate(const std::string& verb) const
{
    return infl_imperate.inflect(verb);
}

Inflector English::infl_imperate({
        {"can",                          "can"},
        {"be",                           "is"},
        {"have",                         "has"},
        {"say",                          "says"},
        {"don\'t",                       "doesn\'t"},
        {"(.*[bcdfghjklmnpqrstvwxyz]o)", "$1es"},
        {"(.*(z|s|ch|sh|j|zh))",         "$1es"},
        {"(.*[bcdfghjklmnpqrstvwxyz])y", "$1ies"},
        {"(.*)",                         "$1s"},
    }, true);

Inflector English::infl_genitive = {
    {"(.*s)^", "$1'"},
    {"(.*)^", "$1's"},
};


} // namespace string