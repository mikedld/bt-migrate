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

#include "BencodeCodec.h"
#include "Exception.h"
#include "IStructuredDataCodec.h"
#include "Throw.h"
#include "Util.h"

#include <boost/filesystem/path.hpp>

#include <sstream>

namespace fs = boost::filesystem;

namespace
{

std::string CalculateInfoHash(Json::Value const& torrent)
{
    if (!torrent.isMember("info"))
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

TorrentInfo::TorrentInfo(Json::Value const& torrent) :
    m_torrent(torrent),
    m_infoHash(CalculateInfoHash(m_torrent))
{
    //
}

void TorrentInfo::ToStream(std::ostream& stream, IStructuredDataCodec const& codec) const
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

    Json::Value const& info = m_torrent["info"];

    if (!info.isMember("files"))
    {
        result += info["length"].asUInt64();
    }
    else
    {
        for (Json::Value const& file : info["files"])
        {
            result += file["length"].asUInt64();
        }
    }

    return result;
}

std::uint32_t TorrentInfo::GetPieceSize() const
{
    Json::Value const& info = m_torrent["info"];

    return info["piece length"].asUInt();
}

std::string TorrentInfo::GetName() const
{
    Json::Value const& info = m_torrent["info"];

    return info["name"].asString();
}

fs::path TorrentInfo::GetFilePath(std::size_t fileIndex) const
{
    fs::path result;

    Json::Value const& info = m_torrent["info"];

    if (!info.isMember("files"))
    {
        if (fileIndex != 0)
        {
            Throw<Exception>() << "Torrent file #" << fileIndex << " does not exist";
        }

        result /= GetName();
    }
    else
    {
        Json::Value const& files = info["files"];

        if (fileIndex >= files.size())
        {
            Throw<Exception>() << "Torrent file #" << fileIndex << " does not exist";
        }

        for (Json::Value const& pathPart : files[static_cast<Json::ArrayIndex>(fileIndex)]["path"])
        {
            result /= pathPart.asString();
        }
    }

    return result;
}

TorrentInfo TorrentInfo::FromStream(std::istream& stream, IStructuredDataCodec const& codec)
{
    Json::Value torrent;
    codec.Decode(stream, torrent);
    return TorrentInfo(torrent);
}
