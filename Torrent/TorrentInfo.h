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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>

using jsoncons::ojson;

class IStructuredDataCodec;

class TorrentInfo
{
public:
    TorrentInfo();
    TorrentInfo(ojson const& torrent);

    void Encode(std::ostream& stream, IStructuredDataCodec const& codec) const;

    std::string const& GetInfoHash() const;
    std::uint64_t GetTotalSize() const;
    std::uint32_t GetPieceSize() const;
    std::string GetName() const;
    std::filesystem::path GetFilePath(std::size_t fileIndex) const;

    void SetTrackers(std::vector<std::vector<std::string>> const& trackers);

    static TorrentInfo Decode(std::istream& stream, IStructuredDataCodec const& codec);

private:
    ojson m_torrent;
    std::string m_infoHash;
};
