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

#include "DelugeStateStore.h"

#include "BencodeCodec.h"
#include "Box.h"
#include "BoxHelper.h"
#include "Exception.h"
#include "IFileStreamProvider.h"
#include "IForwardIterator.h"
#include "PickleCodec.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <json/value.h>
#include <json/writer.h>

#include <cstdlib>
#include <limits>
#include <sstream>

namespace fs = boost::filesystem;

namespace Deluge
{

enum Priority
{
    MinPriority = -6,
    MaxPriority = 6
};

std::string const ConfigDirName = "deluge";
std::string const FastResumeFilename = "torrents.fastresume";
std::string const StateFilename = "torrents.state";

fs::path GetStateDir(fs::path const& configDir)
{
    return configDir / "state";
}

} // namespace Deluge

namespace
{

typedef std::unique_ptr<Json::Value> JsonValuePtr;

Box::LimitInfo FromStoreRatioLimit(Json::Value const& enabled, Json::Value const& storeLimit)
{
    Box::LimitInfo result;
    result.Mode = enabled.asBool() ? Box::LimitMode::Enabled : Box::LimitMode::Inherit;
    result.Value = storeLimit.asDouble();
    return result;
}

Box::LimitInfo FromStoreSpeedLimit(Json::Value const& storeLimit)
{
    Box::LimitInfo result;
    result.Mode = storeLimit.asInt() > 0 ? Box::LimitMode::Enabled :
        (storeLimit.asInt() == 0 ? Box::LimitMode::Disabled : Box::LimitMode::Inherit);
    result.Value = std::max(0, storeLimit.asInt() * 1000);
    return result;
}

std::vector<std::uint64_t> CollectFileSizes(Json::Value const& torrentInfo)
{
    std::vector<std::uint64_t> result;

    if (!torrentInfo.isMember("files"))
    {
        result.push_back(torrentInfo["length"].asUInt64());
    }
    else
    {
        for (Json::Value const& file : torrentInfo["files"])
        {
            result.push_back(file["length"].asUInt64());
        }
    }

    return result;
}

class DelugeTorrentStateIterator : public ITorrentStateIterator
{
public:
    DelugeTorrentStateIterator(fs::path const& stateDir, JsonValuePtr fastResume, JsonValuePtr state,
        IFileStreamProvider& fileStreamProvider);

public:
    // ITorrentStateIterator
    virtual bool GetNext(Box& torrentState);

private:
    fs::path const m_stateDir;
    JsonValuePtr const m_fastResume;
    JsonValuePtr const m_state;
    IFileStreamProvider& m_fileStreamProvider;
    Json::Value::iterator m_stateIt;
    Json::Value::iterator const m_stateEnd;
    BencodeCodec const m_bencoder;
};

DelugeTorrentStateIterator::DelugeTorrentStateIterator(fs::path const& stateDir, JsonValuePtr fastResume, JsonValuePtr state,
    IFileStreamProvider& fileStreamProvider) :
    m_stateDir(stateDir),
    m_fastResume(std::move(fastResume)),
    m_state(std::move(state)),
    m_fileStreamProvider(fileStreamProvider),
    m_stateIt((*m_state)["torrents"].begin()),
    m_stateEnd((*m_state)["torrents"].end()),
    m_bencoder()
{
    //
}

bool DelugeTorrentStateIterator::GetNext(Box& nextBox)
{
    if (m_stateIt == m_stateEnd)
    {
        return false;
    }

    Json::Value const& state = *m_stateIt;
    std::string const infoHash = state["torrent_id"].asString();

    Json::Value fastResume;
    {
        std::istringstream stream((*m_fastResume)[infoHash].asString(), std::ios_base::in | std::ios_base::binary);
        m_bencoder.Decode(stream, fastResume);
    }

    Json::Value torrent;
    {
        ReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(m_stateDir / (infoHash + ".torrent"));
        m_bencoder.Decode(*stream, torrent);
    }

    Box box;
    box.InfoHash = infoHash;
    box.Torrent = torrent;
    box.AddedAt = fastResume["added_time"].asUInt();
    box.CompletedAt = fastResume["completed_time"].asUInt();
    box.IsPaused = state["paused"].asBool();
    box.DownloadedSize = fastResume["total_downloaded"].asUInt64();
    box.UploadedSize = fastResume["total_uploaded"].asUInt64();
    box.CorruptedSize = 0;
    box.SavePath = state["save_path"].asString();
    box.BlockSize = 16 * 1024;
    box.RatioLimit = FromStoreRatioLimit(state["stop_at_ratio"], state["stop_ratio"]);
    box.DownloadSpeedLimit = FromStoreSpeedLimit(state["max_download_speed"]);
    box.UploadSpeedLimit = FromStoreSpeedLimit(state["max_upload_speed"]);

    Json::Value const& filePriorities = state["file_priorities"];
    Json::Value const& currentFileSizes = fastResume["file sizes"];
    std::vector<std::uint64_t> const fileSizes = CollectFileSizes(torrent["info"]);
    for (Json::ArrayIndex i = 0; i < filePriorities.size(); ++i)
    {
        int const filePriority = filePriorities[i].asInt();
        Json::Value const& currentFileSize = currentFileSizes[i];

        Box::FileInfo file;
        file.FullSize = fileSizes[i];
        file.CurrentSize = currentFileSize[0].asUInt64();
        file.DoNotDownload = filePriority == 0;
        file.Priority = filePriority > 0 ? BoxHelper::Priority::FromStore(filePriority - 1, Deluge::MinPriority,
            Deluge::MaxPriority) : Box::NormalPriority;
        file.LastCheckedAt = currentFileSize[1].asUInt();
        box.Files.push_back(std::move(file));
    }

    Json::Value const& pieces = fastResume["pieces"];
    std::size_t const blocksPerPiece = fastResume["blocks per piece"].asUInt();
    std::uint64_t currentFileSize = 0;
    std::size_t currentFileIndex = 0;
    for (char c : pieces.asString())
    {
        std::uint32_t pieceSize = box.BlockSize * blocksPerPiece;
        std::time_t lastCheckedAt = std::numeric_limits<std::time_t>::max();
        while (pieceSize > 0 && currentFileIndex < box.Files.size())
        {
            Box::FileInfo const& file = box.Files.at(currentFileIndex);
            lastCheckedAt = std::min(lastCheckedAt, file.LastCheckedAt);

            std::uint64_t const delta = std::min<std::uint64_t>(file.FullSize - currentFileSize, pieceSize);
            currentFileSize += delta;
            pieceSize -= delta;

            if (pieceSize > 0)
            {
                ++currentFileIndex;
            }
        }

        Box::BlockInfo block;
        block.IsAvailable = c == '\x01';
        block.LastCheckedAt = lastCheckedAt;
        box.Blocks.resize(box.Blocks.size() + blocksPerPiece - pieceSize / box.BlockSize, block);
    }

    // if (box.BlockSize * blocksPerPiece != torrent["info"]["piece length"].asUInt64())
    // {
    //     std::cout << box.BlockSize << " * " << blocksPerPiece << " = " << (box.BlockSize * blocksPerPiece);
    //     std::cout << " != " << torrent["info"]["piece length"].asUInt64();// << std::endl;
    //     std::cout << " (" << pieces.asString().size() << " pieces)";// << std::endl;
    //     std::cout << std::endl;
    //     // throw Exception("Ahha! (" + torrent["info"]["name"].asString() + ")");
    // }

    nextBox = std::move(box);
    ++m_stateIt;
    return true;
}

} // namespace

DelugeStateStore::DelugeStateStore()
{
    //
}

DelugeStateStore::~DelugeStateStore()
{
    //
}

TorrentClient::Enum DelugeStateStore::GetTorrentClient() const
{
    return TorrentClient::Deluge;
}

fs::path DelugeStateStore::GuessConfigDir() const
{
#ifndef _WIN32

    fs::path const homeDir = std::getenv("HOME");

    if (IsValidConfigDir(homeDir / ".config" / Deluge::ConfigDirName))
    {
        return homeDir / ".config" / Deluge::ConfigDirName;
    }

    return fs::path();

#else

    fs::path const appDataDir = std::getenv("APPDATA");

    if (IsValidConfigDir(appDataDir / Deluge::ConfigDirName))
    {
        return appDataDir / Deluge::ConfigDirName;
    }

    return fs::path();

#endif
}

bool DelugeStateStore::IsValidConfigDir(fs::path const& configDir) const
{
    boost::system::error_code dummy;
    fs::path const stateDir = Deluge::GetStateDir(configDir);
    return
        fs::is_regular_file(stateDir / Deluge::FastResumeFilename, dummy) &&
        fs::is_regular_file(stateDir / Deluge::StateFilename, dummy);
}

ITorrentStateIteratorPtr DelugeStateStore::Export(fs::path const& configDir, IFileStreamProvider& fileStreamProvider) const
{
    if (!IsValidConfigDir(configDir))
    {
        throw Exception("Bad Deluge configuration directory: \"" + configDir.string() + "\"");
    }

    fs::path const stateDir = Deluge::GetStateDir(configDir);

    JsonValuePtr fastResume(new Json::Value());
    {
        ReadStreamPtr const stream = fileStreamProvider.GetReadStream(stateDir / Deluge::FastResumeFilename);
        BencodeCodec().Decode(*stream, (*fastResume));
    }

    JsonValuePtr state(new Json::Value());
    {
        ReadStreamPtr const stream = fileStreamProvider.GetReadStream(stateDir / Deluge::StateFilename);
        PickleCodec().Decode(*stream, (*state));
    }

    return ITorrentStateIteratorPtr(new DelugeTorrentStateIterator(stateDir, std::move(fastResume), std::move(state),
        fileStreamProvider));
}

void DelugeStateStore::Import(fs::path const& configDir, ITorrentStateIteratorPtr /*boxes*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    if (!IsValidConfigDir(configDir))
    {
        throw Exception("Bad Deluge configuration directory: \"" + configDir.string() + "\"");
    }

    throw Exception("Not implemented");
}
