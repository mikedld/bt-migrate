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
#include "Logger.h"
#include "Sha1.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <charconv>
#include <filesystem>
#include <format>
#include <iomanip>
#include <locale>
#include <sstream>

namespace fs = std::filesystem;

namespace Util
{

namespace
{

std::string FixPathSeparators(std::string const& nativePath)
{
    if (nativePath.size() >= 3 && std::isalpha(nativePath[0]) && nativePath[1] == ':' &&
        (nativePath[2] == '/' || nativePath[2] == '\\'))
    {
        // Looks like Windows path
        return ReplaceAll(nativePath, "\\", "/");
    }

    return nativePath;
}

} // namespace

fs::path GetPath(std::string const& nativePath)
{
    std::string const fixedPath = FixPathSeparators(nativePath);

    try
    {
        return fs::path{fixedPath};
    }
    catch (std::exception const&)
    {
        Logger(Logger::Warning) << "Path " << std::quoted(fixedPath) << " is invalid";
        return fs::path{fixedPath,  std::locale()};
    }
}

std::string CalculateSha1(std::string const& data)
{
    Sha1 sha;
    sha.Process(data);
    unsigned int result[5];
    sha.GetDigest(result);

    std::ostringstream stream;
    for (unsigned int i : result)
    {
        stream << std::hex << std::setw(8) << std::setfill('0') << i;
    }

    return stream.str();
}

std::string BinaryToHex(std::string const& data)
{
    static char const* const HexAlphabet = "0123456789abcdef";

    std::string result;

    for (char const c : data)
    {
        result += HexAlphabet[(c >> 4) & 0x0f];
        result += HexAlphabet[c & 0x0f];
    }

    return result;
}

void SortJsonObjectKeys(ojson& object)
{
    std::sort(object.object_range().begin(), object.object_range().end(),
        [](auto const& lhs, auto const& rhs) { return lhs.key().compare(rhs.key()) < 0; });
}

std::string GetEnvironmentVariable(std::string const& name, std::string const& defaultValue)
{
    auto const* value = std::getenv(name.c_str());
    return value != nullptr ? value : defaultValue;
}

std::string ReplaceAll(std::string_view str, std::string_view before, std::string_view after)
{
    std::string newString;
    newString.reserve(str.length());

    size_t i = 0, j = 0;

    while ((i = str.find(before, j)) != std::string_view::npos)
    {
        newString.append(str, j, i - j);
        newString.append(after);
        j = i + before.length();
    }

    newString.append(str.substr(j));

    return newString;
}

std::string GetTimestamp(std::string_view fmt)
{
    const auto time = std::time(nullptr);
    std::tm* t = std::localtime(&time);

    char buf[64];
    return { buf, std::strftime(buf, sizeof(buf), fmt.data(), t) };
}

typedef std::string::value_type char_t;

static char_t upChar(char_t c)
{
    return std::use_facet<std::ctype<char_t>>(std::locale()).toupper(c);
}

static char_t lowerChar(char_t c)
{
    return std::use_facet<std::ctype<char_t>>(std::locale()).tolower(c);
}

std::string ToUpper(std::string_view str)
{
    std::string result;
    result.resize(str.size());
    std::transform(str.begin(), str.end(), result.begin(), upChar);
    return result;
}

std::string ToLower(std::string_view str)
{
    std::string result;
    result.resize(str.size());
    std::transform(str.begin(), str.end(), result.begin(), lowerChar);
    return result;
}

static std::string_view g_whitespaces = " \n\r\t\f\v";

static std::string ltrim(std::string_view str)
{
    if (size_t i = str.find_first_not_of(g_whitespaces); i != std::string::npos)
    {
        return std::string(str.substr(i));
    }
    return "";
}

static std::string rtrim(std::string_view str)
{
    if (size_t i = str.find_last_not_of(g_whitespaces); i != std::string::npos)
    {
        return std::string(str.substr(0, i + 1));
    }
    return "";
}

std::string Trim(std::string_view str)
{
    return rtrim(ltrim(str));
}

bool StringEqual(std::string_view a, std::string_view b)
{
    return Trim(ToLower(a)) == Trim(ToLower(b));
}

} // namespace Util
