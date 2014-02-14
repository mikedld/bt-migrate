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
#include <mutex>
#include <sstream>

namespace fs = boost::filesystem;

namespace Detail
{

namespace FastResumeField
{

std::string const AddedTime = "added_time";
std::string const CompletedTime = "completed_time";
std::string const MappedFiles = "mapped_files";
std::string const Pieces = "pieces";
std::string const TotalDownloaded = "total_downloaded";
std::string const TotalUploaded = "total_uploaded";

} // namespace FastResumeField

namespace StateField
{

std::string const Torrents = "torrents";

namespace TorrentField
{

std::string const FilePriorities = "file_priorities";
std::string const MaxDownloadSpeed = "max_download_speed";
std::string const MaxUploadSpeed = "max_upload_speed";
std::string const Paused = "paused";
std::string const SavePath = "save_path";
std::string const StopAtRatio = "stop_at_ratio";
std::string const StopRatio = "stop_ratio";
std::string const TorrentId = "torrent_id";

} // namespace TorrentField

} // namespace StateField

enum Priority
{
    DoNotDownloadPriority = 0,
    MinPriority = -6,
    MaxPriority = 6
};

std::string const DataDirName = "deluge";
std::string const FastResumeFilename = "torrents.fastresume";
std::string const StateFilename = "torrents.state";

fs::path GetStateDir(fs::path const& dataDir)
{
    return dataDir / "state";
}

} // namespace Detail

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

fs::path GetChangedFilePath(Json::Value const& mappedFiles, Json::ArrayIndex index)
{
    fs::path result;

    if (!mappedFiles.isNull())
    {
        fs::path const path = Util::GetPath(mappedFiles[index].asString());
        fs::path::iterator pathIt = path.begin();
        while (++pathIt != path.end())
        {
            result /= *pathIt;
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
    virtual bool GetNext(Box& nextBox);

private:
    fs::path const m_stateDir;
    JsonValuePtr const m_fastResume;
    JsonValuePtr const m_state;
    IFileStreamProvider& m_fileStreamProvider;
    Json::Value::iterator m_stateIt;
    Json::Value::iterator const m_stateEnd;
    std::mutex m_stateItMutex;
    BencodeCodec const m_bencoder;
};

DelugeTorrentStateIterator::DelugeTorrentStateIterator(fs::path const& stateDir, JsonValuePtr fastResume, JsonValuePtr state,
    IFileStreamProvider& fileStreamProvider) :
    m_stateDir(stateDir),
    m_fastResume(std::move(fastResume)),
    m_state(std::move(state)),
    m_fileStreamProvider(fileStreamProvider),
    m_stateIt((*m_state)[Detail::StateField::Torrents].begin()),
    m_stateEnd((*m_state)[Detail::StateField::Torrents].end()),
    m_stateItMutex(),
    m_bencoder()
{
    //
}

bool DelugeTorrentStateIterator::GetNext(Box& nextBox)
{
    namespace FRField = Detail::FastResumeField;
    namespace STField = Detail::StateField::TorrentField;

    std::unique_lock<std::mutex> lock(m_stateItMutex);

    if (m_stateIt == m_stateEnd)
    {
        return false;
    }

    Json::Value const& state = *m_stateIt++;

    lock.unlock();

    std::string const infoHash = state[STField::TorrentId].asString();

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

    box.AddedAt = fastResume[FRField::AddedTime].asUInt();
    box.CompletedAt = fastResume[FRField::CompletedTime].asUInt();
    box.IsPaused = state[STField::Paused].asBool();
    box.DownloadedSize = fastResume[FRField::TotalDownloaded].asUInt64();
    box.UploadedSize = fastResume[FRField::TotalUploaded].asUInt64();
    box.CorruptedSize = 0;
    box.SavePath = Util::GetPath(state[STField::SavePath].asString()) / (fastResume.isMember(FRField::MappedFiles) ?
        *Util::GetPath(fastResume[FRField::MappedFiles][0].asString()).begin() : box.Torrent["name"].asString());
    box.BlockSize = box.Torrent["info"]["piece length"].asUInt();
    box.RatioLimit = FromStoreRatioLimit(state[STField::StopAtRatio], state[STField::StopRatio]);
    box.DownloadSpeedLimit = FromStoreSpeedLimit(state[STField::MaxDownloadSpeed]);
    box.UploadSpeedLimit = FromStoreSpeedLimit(state[STField::MaxUploadSpeed]);

    Json::Value const& filePriorities = state[STField::FilePriorities];
    Json::Value const& mappedFiles = fastResume[FRField::MappedFiles];
    box.Files.reserve(filePriorities.size());
    for (Json::ArrayIndex i = 0; i < filePriorities.size(); ++i)
    {
        int const filePriority = filePriorities[i].asInt();
        fs::path const changedPath = GetChangedFilePath(mappedFiles, i);
        fs::path const originalPath = Util::GetFilePath(box.Torrent, i);

        Box::FileInfo file;
        file.DoNotDownload = filePriority == Detail::DoNotDownloadPriority;
        file.Priority = file.DoNotDownload ? Box::NormalPriority : BoxHelper::Priority::FromStore(filePriority - 1,
            Detail::MinPriority, Detail::MaxPriority);
        file.Path = changedPath == fs::path() || changedPath == originalPath ? fs::path() : changedPath;
        box.Files.push_back(std::move(file));
    }

    std::uint64_t const totalSize = Util::GetTotalTorrentSize(box.Torrent);
    std::uint64_t const totalBlockCount = (totalSize + box.BlockSize - 1) / box.BlockSize;
    box.ValidBlocks.reserve(totalBlockCount);
    for (bool const isPieceValid : fastResume[FRField::Pieces].asString())
    {
        box.ValidBlocks.push_back(isPieceValid);
    }

    nextBox = std::move(box);
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

fs::path DelugeStateStore::GuessDataDir(Intention::Enum intention) const
{
#ifndef _WIN32

    fs::path const homeDir = std::getenv("HOME");

    if (IsValidDataDir(homeDir / ".config" / Detail::DataDirName, intention))
    {
        return homeDir / ".config" / Detail::DataDirName;
    }

    return fs::path();

#else

    fs::path const appDataDir = std::getenv("APPDATA");

    if (IsValidDataDir(appDataDir / Detail::DataDirName, intention))
    {
        return appDataDir / Detail::DataDirName;
    }

    return fs::path();

#endif
}

bool DelugeStateStore::IsValidDataDir(fs::path const& dataDir, Intention::Enum /*intention*/) const
{
    fs::path const stateDir = Detail::GetStateDir(dataDir);
    return
        fs::is_regular_file(stateDir / Detail::FastResumeFilename) &&
        fs::is_regular_file(stateDir / Detail::StateFilename);
}

ITorrentStateIteratorPtr DelugeStateStore::Export(fs::path const& dataDir, IFileStreamProvider& fileStreamProvider) const
{
    if (!IsValidDataDir(dataDir, Intention::Export))
    {
        Throw<Exception>() << "Bad Deluge data directory: " << dataDir;
    }

    fs::path const stateDir = Detail::GetStateDir(dataDir);

    JsonValuePtr fastResume(new Json::Value());
    {
        ReadStreamPtr const stream = fileStreamProvider.GetReadStream(stateDir / Detail::FastResumeFilename);
        BencodeCodec().Decode(*stream, *fastResume);
    }

    JsonValuePtr state(new Json::Value());
    {
        ReadStreamPtr const stream = fileStreamProvider.GetReadStream(stateDir / Detail::StateFilename);
        PickleCodec().Decode(*stream, *state);
    }

    return ITorrentStateIteratorPtr(new DelugeTorrentStateIterator(stateDir, std::move(fastResume), std::move(state),
        fileStreamProvider));
}

void DelugeStateStore::Import(fs::path const& dataDir, ITorrentStateIteratorPtr /*boxes*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    if (!IsValidDataDir(dataDir, Intention::Import))
    {
        Throw<Exception>() << "Bad Deluge data directory: " << dataDir;
    }

    throw NotImplementedException(__func__);
}
