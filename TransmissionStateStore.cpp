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

#include "BencodeCodec.h"
#include "Box.h"
#include "BoxHelper.h"
#include "Exception.h"
#include "IFileStreamProvider.h"
#include "IForwardIterator.h"
#include "Throw.h"
#include "Util.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>

#include <json/value.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>

namespace fs = boost::filesystem;

namespace Transmission
{

enum Priority
{
    MinPriority = -1,
    MaxPriority = 1
};

std::string const CommonConfigDirName = "transmission";
std::string const DaemonConfigDirName = "transmission-daemon";

std::uint32_t const BlockSize = 16 * 1024;

fs::path GetResumeDir(fs::path const& configDir)
{
    return configDir / "resume";
}

fs::path GetResumeFilePath(fs::path const& configDir, std::string const& basename)
{
    return GetResumeDir(configDir) / (basename + ".resume");
}

fs::path GetTorrentsDir(fs::path const& configDir)
{
    return configDir / "torrents";
}

fs::path GetTorrentFilePath(fs::path const& configDir, std::string const& basename)
{
    return GetTorrentsDir(configDir) / (basename + ".torrent");
}

} // namespace Transmission

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
        result.append(BoxHelper::Priority::ToStore(file.Priority, Transmission::MinPriority, Transmission::MaxPriority));
    }
    return result;
}

Json::Value ToStoreProgress(std::vector<bool> const& validBlocks, std::uint32_t blockSize, std::uint64_t totalSize,
    std::size_t fileCount)
{
    std::size_t const validBlockCount = std::count(validBlocks.begin(), validBlocks.end(), true);

    Json::Value result;
    if (validBlockCount == validBlocks.size())
    {
        result["blocks"] = "all";
        result["have"] = "all";
    }
    else if (validBlockCount == 0)
    {
        result["blocks"] = "none";
    }
    else
    {
        std::uint32_t const trBlocksPerBlock = blockSize / Transmission::BlockSize;

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

        trBlocks.resize(((totalSize + Transmission::BlockSize - 1) / Transmission::BlockSize + 7) / 8);

        result["blocks"] = trBlocks;
    }

    Json::Int64 const timeChecked = std::time(nullptr);
    result["time-checked"] = Json::arrayValue;
    for (std::size_t i = 0; i < fileCount; ++i)
    {
        result["time-checked"].append(timeChecked);
    }

    return result;
}

Json::Value ToStoreRatioLimit(Box::LimitInfo const& boxLimit)
{
    Json::Value result;
    result["ratio-mode"] = boxLimit.Mode == Box::LimitMode::Inherit ? 0 : (boxLimit.Mode == Box::LimitMode::Enabled ? 1 : 2);
    result["ratio-limit"] = boost::str(boost::format("%.06f") % boxLimit.Value);
    return result;
}

Json::Value ToStoreSpeedLimit(Box::LimitInfo const& boxLimit)
{
    Json::Value result;
    result["speed-Bps"] = static_cast<int>(boxLimit.Value);
    result["use-global-speed-limit"] = boxLimit.Mode != Box::LimitMode::Disabled ? 1 : 0;
    result["use-speed-limit"] = boxLimit.Mode == Box::LimitMode::Enabled ? 1 : 0;
    return result;
}

void ImportImpl(fs::path const& configDir, ITorrentStateIterator& boxes, IFileStreamProvider& fileStreamProvider)
{
    BencodeCodec const bencoder;
    Box box;
    while (boxes.GetNext(box))
    {
        if (box.BlockSize % Transmission::BlockSize != 0)
        {
            // Transmission doesn't support piece lengths which are not power of two (see trac #4005)
            continue;
        }

        Json::Value resume;

        //resume["activity-date"] = 0;
        resume["added-date"] = static_cast<Json::Int64>(box.AddedAt);
        //resume["bandwidth-priority"] = 0;
        resume["corrupt"] = static_cast<Json::UInt64>(box.CorruptedSize);
        resume["destination"] = box.SavePath;
        resume["dnd"] = ToStoreDoNotDownload(box.Files);
        resume["done-date"] = static_cast<Json::Int64>(box.CompletedAt);
        resume["downloaded"] = static_cast<Json::UInt64>(box.DownloadedSize);
        //resume["downloading-time-seconds"] = 0;
        //resume["idle-limit"] = Json::objectValue;
        //resume["max-peers"] = 5;
        resume["name"] = box.Torrent["info"]["name"];
        resume["paused"] = box.IsPaused ? 1 : 0;
        //resume["peers2"] = "";
        resume["priority"] = ToStorePriority(box.Files);
        resume["progress"] = ToStoreProgress(box.ValidBlocks, box.BlockSize, Util::GetTotalTorrentSize(box.Torrent),
            box.Files.size());
        resume["ratio-limit"] = ToStoreRatioLimit(box.RatioLimit);
        //resume["seeding-time-seconds"] = 0;
        resume["speed-limit-down"] = ToStoreSpeedLimit(box.DownloadSpeedLimit);
        resume["speed-limit-up"] = ToStoreSpeedLimit(box.UploadSpeedLimit);
        resume["uploaded"] = static_cast<Json::UInt64>(box.UploadedSize);

        std::string const baseName = resume["name"].asString() + '.' + box.InfoHash.substr(0, 16);

        try
        {
            WriteStreamPtr stream;

            stream = fileStreamProvider.GetWriteStream(Transmission::GetTorrentFilePath(configDir, baseName));
            bencoder.Encode(*stream, box.Torrent);

            stream = fileStreamProvider.GetWriteStream(Transmission::GetResumeFilePath(configDir, baseName));
            bencoder.Encode(*stream, resume);
        }
        catch (std::exception const& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

} // namespace

TransmissionStateStore::TransmissionStateStore()
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

fs::path TransmissionStateStore::GuessConfigDir() const
{
#ifndef _WIN32

    fs::path const homeDir = std::getenv("HOME");

    if (IsValidConfigDir(homeDir / ".config" / Transmission::CommonConfigDirName))
    {
        return homeDir / ".config" / Transmission::CommonConfigDirName;
    }

    if (IsValidConfigDir(homeDir / ".config" / Transmission::DaemonConfigDirName))
    {
        return homeDir / ".config" / Transmission::DaemonConfigDirName;
    }

    return fs::path();

#else

    throw NotImplementedException(__func__);

#endif
}

bool TransmissionStateStore::IsValidConfigDir(fs::path const& configDir) const
{
    boost::system::error_code dummy;
    return
        fs::is_directory(Transmission::GetResumeDir(configDir), dummy) &&
        fs::is_directory(Transmission::GetTorrentsDir(configDir), dummy);
}

ITorrentStateIteratorPtr TransmissionStateStore::Export(fs::path const& configDir,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    if (!IsValidConfigDir(configDir))
    {
        Throw<Exception>() << "Bad Transmission configuration directory: " << configDir;
    }

    throw NotImplementedException(__func__);
}

void TransmissionStateStore::Import(fs::path const& configDir, ITorrentStateIteratorPtr boxes,
    IFileStreamProvider& fileStreamProvider) const
{
    if (!IsValidConfigDir(configDir))
    {
        Throw<Exception>() << "Bad Transmission configuration directory: " << configDir;
    }

    unsigned int const threadCount = std::max(1u, std::thread::hardware_concurrency());

    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back(&ImportImpl, std::cref(configDir), std::ref(*boxes), std::ref(fileStreamProvider));
    }

    for (std::thread& thread : threads)
    {
        thread.join();
    }
}
