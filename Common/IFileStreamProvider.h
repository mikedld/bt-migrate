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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iosfwd>
#include <memory>

namespace fs = std::filesystem;

typedef std::unique_ptr<std::istream> IReadStreamPtr;
typedef std::unique_ptr<std::ostream> IWriteStreamPtr;

class IFileStreamProvider
{
public:
    virtual ~IFileStreamProvider() noexcept(false);

    virtual IReadStreamPtr GetReadStream(fs::path const& path) const = 0;
    virtual IWriteStreamPtr GetWriteStream(fs::path const& path) = 0;
};
