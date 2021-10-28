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

#pragma once

#include "Exception.h"

#include <jsoncons/json.hpp>

#include <charconv>
#include <filesystem>
#include <format>
#include <string>

namespace fs = std::filesystem;
using jsoncons::ojson;

namespace Util
{

template<typename T>
T StringToNumber(std::string_view str) requires std::is_integral_v<T> || std::is_floating_point_v<T>
{
	T value;
	if (auto [_, ec] = std::from_chars(str.data(), str.data() + str.size(), value); ec != std::errc())
	{
		throw Exception(std::format("Unable to convert \"{}\" to number", str));
	}
	return value;
}

fs::path GetPath(std::string const& nativePath);

std::string CalculateSha1(std::string const& data);

std::string BinaryToHex(std::string const& data);

void SortJsonObjectKeys(ojson& object);

std::string GetEnvironmentVariable(std::string const& name, std::string const& defaultValue);

std::string ReplaceAll(std::string_view str, std::string_view before, std::string_view after);

std::string GetTimestamp(std::string_view fmt = "%F_%T");

std::string ToUpper(std::string_view str);

std::string ToLower(std::string_view str);

std::string Trim(std::string_view str);

bool StringEqual(std::string_view a, std::string_view b);

} // namespace Util
