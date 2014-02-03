// bt-migrate, torrent state migration tool
// Copyright (C) 2014 Mike Gelfand <mikedld@mikedld.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "Util.h"

#include "Exception.h"

#include <cerrno>
#include <cstdlib>

namespace Util
{

long long StringToInt(std::string const& text)
{
    errno = 0;
    long long const result = std::strtoll(text.c_str(), NULL, 10);
    if (result == 0 && errno != 0)
    {
        throw Exception("Unable to convert \"" + text + "\" to integer");
    }

    return result;
}

} // namespace Util
