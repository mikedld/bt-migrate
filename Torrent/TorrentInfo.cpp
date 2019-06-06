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

#include "TorrentInfo.h"

#include "Codec/BencodeCodec.h"
#include "Codec/IStructuredDataCodec.h"
#include "Common/Exception.h"
#include "Common/Throw.h"
#include "Common/Util.h"

#include <boost/filesystem/path.hpp>

#include <sstream>

namespace fs = boost::filesystem;

namespace
{

std::string CalculateInfoHash(ojson const& torrent)
{
    if (!torrent.contains("info"))
    {
        throw Exception("Torrent file is missing info dictionary");
    }

    std::ostringstream infoStream;
    BencodeCodec().Encode(infoStream, torrent["info"]);
    return Util::CalculateSha1(infoStream.str());
}

} // namespace

TorrentInfo::TorrentInfo() :
    m_torrent(),
    m_infoHash()
{
    //
}

TorrentInfo::TorrentInfo(ojson const& torrent) :
    m_torrent(torrent),
    m_infoHash(CalculateInfoHash(m_torrent))
{
    //
}

void TorrentInfo::Encode(std::ostream& stream, IStructuredDataCodec const& codec) const
{
    codec.Encode(stream, m_torrent);
}

std::string const& TorrentInfo::GetInfoHash() const
{
    return m_infoHash;
}

std::uint64_t TorrentInfo::GetTotalSize() const
{
    std::uint64_t result = 0;

    ojson const& info = m_torrent["info"];

    if (!info.contains("files"))
    {
        result += info["length"].as<std::uint64_t>();
    }
    else
    {
        for (ojson const& file : info["files"].array_range())
        {
            result += file["length"].as<std::uint64_t>();
        }
    }

    return result;
}

std::uint32_t TorrentInfo::GetPieceSize() const
{
    ojson const& info = m_torrent["info"];

    return info["piece length"].as<std::uint32_t>();
}

std::string TorrentInfo::GetName() const
{
    ojson const& info = m_torrent["info"];

    return info["name"].as<std::string>();
}

fs::path TorrentInfo::GetFilePath(std::size_t fileIndex) const
{
    fs::path result;

    ojson const& info = m_torrent["info"];

    if (!info.contains("files"))
    {
        if (fileIndex != 0)
        {
            Throw<Exception>() << "Torrent file #" << fileIndex << " does not exist";
        }

        result /= GetName();
    }
    else
    {
        ojson const& files = info["files"];

        if (fileIndex >= files.size())
        {
            Throw<Exception>() << "Torrent file #" << fileIndex << " does not exist";
        }

        for (ojson const& pathPart : files[fileIndex]["path"].array_range())
        {
            result /= pathPart.as<std::string>();
        }
    }

    return result;
}

void TorrentInfo::SetTrackers(std::vector<std::vector<std::string>> const& trackers)
{
    ojson announceList = ojson::array();

    for (auto const& tier : trackers)
    {
        announceList.emplace_back(tier);
    }

    m_torrent["announce-list"] = announceList;

    if (announceList.empty())
    {
        m_torrent.erase("announce");
    }
    else
    {
        m_torrent["announce"] = announceList[0][0];
    }

    Util::SortJsonObjectKeys(m_torrent);
}

TorrentInfo TorrentInfo::Decode(std::istream& stream, IStructuredDataCodec const& codec)
{
    ojson torrent;
    codec.Decode(stream, torrent);
    return TorrentInfo(torrent);
}
