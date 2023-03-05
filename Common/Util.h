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

#include <jsoncons/json.hpp>

#include <filesystem>
#include <locale>
#include <string>
#include <string_view>

using jsoncons::ojson;

namespace Util
{

long long StringToInt(std::string const& text);

std::filesystem::path GetPath(std::string const& nativePath);

std::string CalculateSha1(std::string const& data);

std::string BinaryToHex(std::string const& data);

void SortJsonObjectKeys(ojson& object);

std::string GetEnvironmentVariable(std::string const& name, std::string const& defaultValue);

bool IsEqualNoCase(std::string_view lhs, std::string_view rhs, std::locale const& locale = {});

} // namespace Util
