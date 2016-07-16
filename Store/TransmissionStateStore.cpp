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

#include "TransmissionStateStore.h"

#include "Common/Exception.h"
#include "Common/IFileStreamProvider.h"
#include "Common/IForwardIterator.h"
#include "Common/Throw.h"
#include "Common/Util.h"
#include "Torrent/Box.h"
#include "Torrent/BoxHelper.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>

#include <json/value.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace fs = boost::filesystem;

namespace Detail
{

namespace ResumeField
{

std::string const AddedDate = "added-date";
std::string const Corrupt = "corrupt";
std::string const Destination = "destination";
std::string const Dnd = "dnd";
std::string const DoneDate = "done-date";
std::string const Downloaded = "downloaded";
std::string const Name = "name";
std::string const Paused = "paused";
std::string const Priority = "priority";
std::string const Progress = "progress";
std::string const RatioLimit = "ratio-limit";
std::string const SpeedLimitDown = "speed-limit-down";
std::string const SpeedLimitUp = "speed-limit-up";
std::string const Uploaded = "uploaded";

namespace ProgressField
{

std::string const Blocks = "blocks";
std::string const Have = "have";
std::string const TimeChecked = "time-checked";

} // namespace ProgressField

namespace RatioLimitField
{

std::string const RatioMode = "ratio-mode";
std::string const RatioLimit = "ratio-limit";

} // namespace RatioLimitField

namespace SpeedLimitField
{

std::string const SpeedBps = "speed-Bps";
std::string const UseGlobalSpeedLimit = "use-global-speed-limit";
std::string const UseSpeedLimit = "use-speed-limit";

} // namespace SpeedLimitField

} // namespace ResumeField

enum Priority
{
    MinPriority = -1,
    MaxPriority = 1
};

std::string const CommonDataDirName = "transmission";
std::string const DaemonDataDirName = "transmission-daemon";

std::uint32_t const BlockSize = 16 * 1024;

fs::path GetResumeDir(fs::path const& dataDir)
{
    return dataDir / "resume";
}

fs::path GetResumeFilePath(fs::path const& dataDir, std::string const& basename)
{
    return GetResumeDir(dataDir) / (basename + ".resume");
}

fs::path GetTorrentsDir(fs::path const& dataDir)
{
    return dataDir / "torrents";
}

fs::path GetTorrentFilePath(fs::path const& dataDir, std::string const& basename)
{
    return GetTorrentsDir(dataDir) / (basename + ".torrent");
}

} // namespace Detail

namespace
{

Json::Value ToStoreDoNotDownload(std::vector<Box::FileInfo> const& files)
{
    Json::Value result = Json::arrayValue;
    for (Box::FileInfo const& file : files)
    {
        result.append(file.DoNotDownload ? 1 : 0);
    }
    return result;
}

Json::Value ToStorePriority(std::vector<Box::FileInfo> const& files)
{
    Json::Value result = Json::arrayValue;
    for (Box::FileInfo const& file : files)
    {
        result.append(BoxHelper::Priority::ToStore(file.Priority, Detail::MinPriority, Detail::MaxPriority));
    }
    return result;
}

Json::Value ToStoreProgress(std::vector<bool> const& validBlocks, std::uint32_t blockSize, std::uint64_t totalSize,
    std::size_t fileCount)
{
    namespace RPField = Detail::ResumeField::ProgressField;

    std::size_t const validBlockCount = std::count(validBlocks.begin(), validBlocks.end(), true);

    Json::Value result;
    if (validBlockCount == validBlocks.size())
    {
        result[RPField::Blocks] = "all";
        result[RPField::Have] = "all";
    }
    else if (validBlockCount == 0)
    {
        result[RPField::Blocks] = "none";
    }
    else
    {
        std::uint32_t const trBlocksPerBlock = blockSize / Detail::BlockSize;

        std::string trBlocks;
        trBlocks.reserve((validBlocks.size() * trBlocksPerBlock + 7) / 8);

        std::uint8_t blockPack = 0;
        std::int8_t blockPackShift = 7;
        for (bool const isValidBlock : validBlocks)
        {
            for (std::uint32_t i = 0; i < trBlocksPerBlock; ++i)
            {
                blockPack |= (isValidBlock ? 1 : 0) << blockPackShift;
                if (--blockPackShift < 0)
                {
                    trBlocks += static_cast<char>(blockPack);
                    blockPack = 0;
                    blockPackShift = 7;
                }
            }
        }

        if (blockPackShift < 7)
        {
            trBlocks += static_cast<char>(blockPack);
        }

        trBlocks.resize(((totalSize + Detail::BlockSize - 1) / Detail::BlockSize + 7) / 8);

        result[RPField::Blocks] = trBlocks;
    }

    Json::Int64 const timeChecked = std::time(nullptr);
    result[RPField::TimeChecked] = Json::arrayValue;
    for (std::size_t i = 0; i < fileCount; ++i)
    {
        result[RPField::TimeChecked].append(timeChecked);
    }

    return result;
}

Json::Value ToStoreRatioLimit(Box::LimitInfo const& boxLimit)
{
    namespace RRLField = Detail::ResumeField::RatioLimitField;

    Json::Value result;
    result[RRLField::RatioMode] = boxLimit.Mode == Box::LimitMode::Inherit ? 0 :
        (boxLimit.Mode == Box::LimitMode::Enabled ? 1 : 2);
    result[RRLField::RatioLimit] = boost::str(boost::format("%.06f") % boxLimit.Value);
    return result;
}

Json::Value ToStoreSpeedLimit(Box::LimitInfo const& boxLimit)
{
    namespace RSLField = Detail::ResumeField::SpeedLimitField;

    Json::Value result;
    result[RSLField::SpeedBps] = static_cast<int>(boxLimit.Value);
    result[RSLField::UseGlobalSpeedLimit] = boxLimit.Mode != Box::LimitMode::Disabled ? 1 : 0;
    result[RSLField::UseSpeedLimit] = boxLimit.Mode == Box::LimitMode::Enabled ? 1 : 0;
    return result;
}

} // namespace

TransmissionStateStore::TransmissionStateStore() :
    m_bencoder()
{
    //
}

TransmissionStateStore::~TransmissionStateStore()
{
    //
}

TorrentClient::Enum TransmissionStateStore::GetTorrentClient() const
{
    return TorrentClient::Transmission;
}

fs::path TransmissionStateStore::GuessDataDir(Intention::Enum intention) const
{
#ifndef _WIN32

    fs::path const homeDir = std::getenv("HOME");

    if (IsValidDataDir(homeDir / ".config" / Detail::CommonDataDirName, intention))
    {
        return homeDir / ".config" / Detail::CommonDataDirName;
    }

    if (IsValidDataDir(homeDir / ".config" / Detail::DaemonDataDirName, intention))
    {
        return homeDir / ".config" / Detail::DaemonDataDirName;
    }

    return fs::path();

#else

    throw NotImplementedException(__func__);

#endif
}

bool TransmissionStateStore::IsValidDataDir(fs::path const& dataDir, Intention::Enum /*intention*/) const
{
    return
        fs::is_directory(Detail::GetResumeDir(dataDir)) &&
        fs::is_directory(Detail::GetTorrentsDir(dataDir));
}

ITorrentStateIteratorPtr TransmissionStateStore::Export(fs::path const& /*dataDir*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    throw NotImplementedException(__func__);
}

void TransmissionStateStore::Import(fs::path const& dataDir, Box const& box, IFileStreamProvider& fileStreamProvider) const
{
    namespace RField = Detail::ResumeField;

    if (box.BlockSize % Detail::BlockSize != 0)
    {
        // See trac #4005.
        Throw<ImportCancelledException>() << "Transmission does not support torrents with piece length not multiple of two: " <<
            box.BlockSize;
    }

    for (Box::FileInfo const& file : box.Files)
    {
        if (!file.Path.is_relative())
        {
            Throw<ImportCancelledException>() << "Transmission does not support moving files outside of download directory: " <<
                file.Path;
        }
    }

    Json::Value resume;

    //resume["activity-date"] = 0;
    resume[RField::AddedDate] = static_cast<Json::Int64>(box.AddedAt);
    //resume["bandwidth-priority"] = 0;
    resume[RField::Corrupt] = static_cast<Json::UInt64>(box.CorruptedSize);
    resume[RField::Destination] = box.SavePath.parent_path().string();
    resume[RField::Dnd] = ToStoreDoNotDownload(box.Files);
    resume[RField::DoneDate] = static_cast<Json::Int64>(box.CompletedAt);
    resume[RField::Downloaded] = static_cast<Json::UInt64>(box.DownloadedSize);
    //resume["downloading-time-seconds"] = 0;
    //resume["idle-limit"] = Json::objectValue;
    //resume["max-peers"] = 5;
    resume[RField::Name] = box.SavePath.filename().string();
    resume[RField::Paused] = box.IsPaused ? 1 : 0;
    //resume["peers2"] = "";
    resume[RField::Priority] = ToStorePriority(box.Files);
    resume[RField::Progress] = ToStoreProgress(box.ValidBlocks, box.BlockSize, box.Torrent.GetTotalSize(), box.Files.size());
    resume[RField::RatioLimit] = ToStoreRatioLimit(box.RatioLimit);
    //resume["seeding-time-seconds"] = 0;
    resume[RField::SpeedLimitDown] = ToStoreSpeedLimit(box.DownloadSpeedLimit);
    resume[RField::SpeedLimitUp] = ToStoreSpeedLimit(box.UploadSpeedLimit);
    resume[RField::Uploaded] = static_cast<Json::UInt64>(box.UploadedSize);

    std::string const baseName = resume[RField::Name].asString() + '.' + box.Torrent.GetInfoHash().substr(0, 16);

    {
        WriteStreamPtr const stream = fileStreamProvider.GetWriteStream(Detail::GetTorrentFilePath(dataDir, baseName));
        box.Torrent.Encode(*stream, m_bencoder);
    }

    {
        WriteStreamPtr const stream = fileStreamProvider.GetWriteStream(Detail::GetResumeFilePath(dataDir, baseName));
        m_bencoder.Encode(*stream, resume);
    }
}
