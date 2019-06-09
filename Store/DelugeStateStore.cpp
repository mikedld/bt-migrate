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

#include "Codec/BencodeCodec.h"
#include "Codec/PickleCodec.h"
#include "Common/Exception.h"
#include "Common/IFileStreamProvider.h"
#include "Common/IForwardIterator.h"
#include "Common/Logger.h"
#include "Common/Throw.h"
#include "Common/Util.h"
#include "Torrent/Box.h"
#include "Torrent/BoxHelper.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <jsoncons/json.hpp>

#include <cstdlib>
#include <limits>
#include <mutex>
#include <sstream>

namespace fs = boost::filesystem;

namespace
{
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
std::string const Trackers = "trackers";

namespace TrackerField
{

std::string const Tier = "tier";
std::string const Url = "url";

} // namespace TrackersField

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
} // namespace

namespace
{

Box::LimitInfo FromStoreRatioLimit(ojson const& enabled, ojson const& storeLimit)
{
    Box::LimitInfo result;
    result.Mode = enabled.as<bool>() ? Box::LimitMode::Enabled : Box::LimitMode::Inherit;
    result.Value = storeLimit.as<double>();
    return result;
}

Box::LimitInfo FromStoreSpeedLimit(ojson const& storeLimit)
{
    Box::LimitInfo result;
    result.Mode = storeLimit.as<int>() > 0 ? Box::LimitMode::Enabled :
        (storeLimit.as<int>() == 0 ? Box::LimitMode::Disabled : Box::LimitMode::Inherit);
    result.Value = std::max<decltype(result.Value)>(0, storeLimit.as<int>() * 1000.);
    return result;
}

fs::path GetChangedFilePath(ojson const& mappedFiles, std::size_t index)
{
    fs::path result;

    if (!mappedFiles.is_null())
    {
        fs::path const path = Util::GetPath(mappedFiles[index].as<std::string>());
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
    DelugeTorrentStateIterator(fs::path const& stateDir, ojson&& fastResume, ojson&& state,
        IFileStreamProvider const& fileStreamProvider);

public:
    // ITorrentStateIterator
    virtual bool GetNext(Box& nextBox);

private:
    fs::path const m_stateDir;
    ojson const m_fastResume;
    ojson const m_state;
    IFileStreamProvider const& m_fileStreamProvider;
    ojson::const_array_iterator m_stateIt;
    ojson::const_array_iterator const m_stateEnd;
    std::mutex m_stateItMutex;
    BencodeCodec const m_bencoder;
};

DelugeTorrentStateIterator::DelugeTorrentStateIterator(fs::path const& stateDir, ojson&& fastResume, ojson&& state,
    IFileStreamProvider const& fileStreamProvider) :
    m_stateDir(stateDir),
    m_fastResume(std::move(fastResume)),
    m_state(std::move(state)),
    m_fileStreamProvider(fileStreamProvider),
    m_stateIt(m_state[Detail::StateField::Torrents].array_range().begin()),
    m_stateEnd(m_state[Detail::StateField::Torrents].array_range().end()),
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

    ojson const& state = *m_stateIt++;

    lock.unlock();

    std::string const infoHash = state[STField::TorrentId].as<std::string>();

    ojson fastResume;
    {
        std::istringstream stream(m_fastResume[infoHash].as<std::string>(), std::ios_base::in | std::ios_base::binary);
        m_bencoder.Decode(stream, fastResume);
    }

    Box box;

    {
        IReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(m_stateDir / (infoHash + ".torrent"));
        box.Torrent = TorrentInfo::Decode(*stream, m_bencoder);
    }

    if (box.Torrent.GetInfoHash() != infoHash)
    {
        Throw<Exception>() << "Info hashes don't match: " << box.Torrent.GetInfoHash() << " vs. " << infoHash;
    }

    box.AddedAt = fastResume[FRField::AddedTime].as<std::time_t>();
    box.CompletedAt = fastResume[FRField::CompletedTime].as<std::time_t>();
    box.IsPaused = state[STField::Paused].as<bool>();
    box.DownloadedSize = fastResume[FRField::TotalDownloaded].as<std::uint64_t>();
    box.UploadedSize = fastResume[FRField::TotalUploaded].as<std::uint64_t>();
    box.CorruptedSize = 0;
    box.SavePath = Util::GetPath(state[STField::SavePath].as<std::string>()) / (fastResume.contains(FRField::MappedFiles) ?
        *Util::GetPath(fastResume[FRField::MappedFiles][0].as<std::string>()).begin() : box.Torrent.GetName());
    box.BlockSize = box.Torrent.GetPieceSize();
    box.RatioLimit = FromStoreRatioLimit(state[STField::StopAtRatio], state[STField::StopRatio]);
    box.DownloadSpeedLimit = FromStoreSpeedLimit(state[STField::MaxDownloadSpeed]);
    box.UploadSpeedLimit = FromStoreSpeedLimit(state[STField::MaxUploadSpeed]);

    ojson const& filePriorities = state[STField::FilePriorities];
    ojson const& mappedFiles = fastResume.get_with_default(FRField::MappedFiles, ojson::null());
    box.Files.reserve(filePriorities.size());
    for (std::size_t i = 0; i < filePriorities.size(); ++i)
    {
        int const filePriority = filePriorities[i].as<int>();
        fs::path const changedPath = GetChangedFilePath(mappedFiles, i);
        fs::path const originalPath = box.Torrent.GetFilePath(i);

        Box::FileInfo file;
        file.DoNotDownload = filePriority == Detail::DoNotDownloadPriority;
        file.Priority = file.DoNotDownload ? Box::NormalPriority : BoxHelper::Priority::FromStore(filePriority - 1,
            Detail::MinPriority, Detail::MaxPriority);
        file.Path = changedPath == fs::path() || changedPath == originalPath ? fs::path() : changedPath;
        box.Files.push_back(std::move(file));
    }

    std::uint64_t const totalSize = box.Torrent.GetTotalSize();
    std::uint64_t const totalBlockCount = (totalSize + box.BlockSize - 1) / box.BlockSize;
    box.ValidBlocks.reserve(totalBlockCount);
    for (bool const isPieceValid : fastResume[FRField::Pieces].as<std::string>())
    {
        box.ValidBlocks.push_back(isPieceValid);
    }

    for (ojson const& tracker : state[STField::Trackers].array_range())
    {
        namespace tf = STField::TrackerField;

        std::size_t const tier = tracker[tf::Tier].as<std::size_t>();
        std::string const url = tracker[tf::Url].as<std::string>();

        box.Trackers.resize(std::max(box.Trackers.size(), tier + 1));
        box.Trackers[tier].push_back(url);
    }

    nextBox = std::move(box);
    return true;
}

} // namespace

DelugeStateStore::DelugeStateStore() = default;
DelugeStateStore::~DelugeStateStore() = default;

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

ITorrentStateIteratorPtr DelugeStateStore::Export(fs::path const& dataDir, IFileStreamProvider const& fileStreamProvider) const
{
    fs::path const stateDir = Detail::GetStateDir(dataDir);

    Logger(Logger::Debug) << "[Deluge] Loading " << Detail::FastResumeFilename;

    ojson fastResume;
    {
        IReadStreamPtr const stream = fileStreamProvider.GetReadStream(stateDir / Detail::FastResumeFilename);
        BencodeCodec().Decode(*stream, fastResume);
    }

    Logger(Logger::Debug) << "[Deluge] Loading " << Detail::StateFilename;

    ojson state;
    {
        IReadStreamPtr const stream = fileStreamProvider.GetReadStream(stateDir / Detail::StateFilename);
        PickleCodec().Decode(*stream, state);
    }

    return std::make_unique<DelugeTorrentStateIterator>(stateDir, std::move(fastResume), std::move(state), fileStreamProvider);
}

void DelugeStateStore::Import(fs::path const& /*dataDir*/, Box const& /*box*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    throw NotImplementedException(__func__);
}
