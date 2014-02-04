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
#include "Throw.h"
#include "Util.h"

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
    DoNotDownloadPriority = 0,
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

class DelugeTorrentStateIterator : public ITorrentStateIterator
{
public:
    DelugeTorrentStateIterator(fs::path const& stateDir, JsonValuePtr fastResume, JsonValuePtr state,
        IFileStreamProvider& fileStreamProvider);

public:
    // ITorrentStateIterator
    virtual bool GetNext(Box& nextBox);

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

    Box box;

    {
        ReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(m_stateDir / (infoHash + ".torrent"));
        BoxHelper::LoadTorrent(*stream, box);
    }

    if (box.InfoHash != infoHash)
    {
        Throw<Exception>() << "Info hashes don't match: " << box.InfoHash << " vs. " << infoHash;
    }

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
    for (Json::ArrayIndex i = 0; i < filePriorities.size(); ++i)
    {
        int const filePriority = filePriorities[i].asInt();

        Box::FileInfo file;
        file.DoNotDownload = filePriority == Deluge::DoNotDownloadPriority;
        file.Priority = file.DoNotDownload ? Box::NormalPriority : BoxHelper::Priority::FromStore(filePriority - 1,
            Deluge::MinPriority, Deluge::MaxPriority);
        box.Files.push_back(std::move(file));
    }

    std::uint32_t const torrentPieceSize = box.Torrent["info"]["piece length"].asUInt64();
    // if (torrentPieceSize % box.BlockSize != 0)
    // {
    //     Throw<Exception>() << "Unsupported torrent piece size (" << torrentPieceSize << ")";
    // }

    std::string const pieces = fastResume["pieces"].asString();
    std::int32_t const blocksPerPiece = torrentPieceSize / box.BlockSize;
    box.ValidBlocks.reserve(pieces.size() * blocksPerPiece);
    for (bool const isPieceValid : pieces)
    {
        box.ValidBlocks.resize(box.ValidBlocks.size() + blocksPerPiece, isPieceValid);
    }

    std::uint64_t const totalSize = Util::GetTotalTorrentSize(box.Torrent);
    std::uint64_t const totalBlockCount = (totalSize + box.BlockSize - 1) / box.BlockSize;
    if (box.ValidBlocks.size() < totalBlockCount)
    {
        throw Exception("Unable to export valid pieces");
    }

    box.ValidBlocks.resize(box.ValidBlocks.size() - (blocksPerPiece - (totalBlockCount % blocksPerPiece)));

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
        Throw<Exception>() << "Bad Deluge configuration directory: " << configDir;
    }

    fs::path const stateDir = Deluge::GetStateDir(configDir);

    JsonValuePtr fastResume(new Json::Value());
    {
        ReadStreamPtr const stream = fileStreamProvider.GetReadStream(stateDir / Deluge::FastResumeFilename);
        BencodeCodec().Decode(*stream, *fastResume);
    }

    JsonValuePtr state(new Json::Value());
    {
        ReadStreamPtr const stream = fileStreamProvider.GetReadStream(stateDir / Deluge::StateFilename);
        PickleCodec().Decode(*stream, *state);
    }

    return ITorrentStateIteratorPtr(new DelugeTorrentStateIterator(stateDir, std::move(fastResume), std::move(state),
        fileStreamProvider));
}

void DelugeStateStore::Import(fs::path const& configDir, ITorrentStateIteratorPtr /*boxes*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    if (!IsValidConfigDir(configDir))
    {
        Throw<Exception>() << "Bad Deluge configuration directory: " << configDir;
    }

    throw NotImplementedException(__func__);
}
