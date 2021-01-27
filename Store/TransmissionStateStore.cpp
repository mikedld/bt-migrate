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
#include "Common/Util.h"
#include "Torrent/Box.h"
#include "Torrent/BoxHelper.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <jsoncons/json.hpp>
#include <pugixml.hpp>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <tuple>

namespace fs = boost::filesystem;

namespace
{
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
std::string const Files = "files";
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
std::string const MacDataDirName = "Transmission";

std::uint32_t const BlockSize = 16 * 1024;

fs::path GetResumeDir(fs::path const& dataDir, TransmissionStateType stateType)
{
    return dataDir / (stateType == TransmissionStateType::Mac ? "Resume" : "resume");
}

fs::path GetResumeFilePath(fs::path const& dataDir, std::string const& basename, TransmissionStateType stateType)
{
    return GetResumeDir(dataDir, stateType) / (basename + ".resume");
}

fs::path GetTorrentsDir(fs::path const& dataDir, TransmissionStateType stateType)
{
    return dataDir / (stateType == TransmissionStateType::Mac ? "Torrents" : "torrents");
}

fs::path GetTorrentFilePath(fs::path const& dataDir, std::string const& basename, TransmissionStateType stateType)
{
    return GetTorrentsDir(dataDir, stateType) / (basename + ".torrent");
}

fs::path GetMacTransfersFilePath(fs::path const& dataDir)
{
    return dataDir / "Transfers.plist";
}

} // namespace Detail

ojson ToStoreDoNotDownload(std::vector<Box::FileInfo> const& files)
{
    ojson result = ojson::array();
    for (Box::FileInfo const& file : files)
    {
        result.push_back(file.DoNotDownload ? 1 : 0);
    }
    return result;
}

ojson ToStorePriority(std::vector<Box::FileInfo> const& files)
{
    ojson result = ojson::array();
    for (Box::FileInfo const& file : files)
    {
        result.push_back(BoxHelper::Priority::ToStore(file.Priority, Detail::MinPriority, Detail::MaxPriority));
    }
    return result;
}

ojson ToStoreProgress(std::vector<bool> const& validBlocks, std::uint32_t blockSize, std::uint64_t totalSize,
    std::size_t fileCount)
{
    namespace RPField = Detail::ResumeField::ProgressField;

    std::size_t const validBlockCount = std::count(validBlocks.begin(), validBlocks.end(), true);

    ojson result = ojson::object();
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

    std::int64_t const timeChecked = std::time(nullptr);
    result[RPField::TimeChecked] = ojson::array();
    for (std::size_t i = 0; i < fileCount; ++i)
    {
        result[RPField::TimeChecked].push_back(timeChecked);
    }

    return result;
}

ojson ToStoreRatioLimit(Box::LimitInfo const& boxLimit)
{
    namespace RRLField = Detail::ResumeField::RatioLimitField;

    ojson result = ojson::object();
    result[RRLField::RatioMode] = boxLimit.Mode == Box::LimitMode::Inherit ? 0 :
        (boxLimit.Mode == Box::LimitMode::Enabled ? 1 : 2);
    result[RRLField::RatioLimit] = boost::str(boost::format("%.06f") % boxLimit.Value);
    return result;
}

ojson ToStoreSpeedLimit(Box::LimitInfo const& boxLimit)
{
    namespace RSLField = Detail::ResumeField::SpeedLimitField;

    ojson result = ojson::object();
    result[RSLField::SpeedBps] = static_cast<int>(boxLimit.Value);
    result[RSLField::UseGlobalSpeedLimit] = boxLimit.Mode != Box::LimitMode::Disabled ? 1 : 0;
    result[RSLField::UseSpeedLimit] = boxLimit.Mode == Box::LimitMode::Enabled ? 1 : 0;
    return result;
}

std::tuple<pugi::xml_document, pugi::xml_node> CreateMacPropertyList()
{
    pugi::xml_document doc;

    auto xmlDecl = doc.append_child(pugi::node_declaration);
    xmlDecl.append_attribute("version") = "1.0";
    xmlDecl.append_attribute("encoding") = "UTF-8";

    auto docType = doc.append_child(pugi::node_doctype);
    docType.set_value(R"(plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd")");

    auto plist = doc.append_child("plist");
    plist.append_attribute("version") = "1.0";

    return {std::move(doc), std::move(plist)};
}

void ToMacStoreTransfer(Box const& box, fs::path const& torrentFilePath, pugi::xml_node& transfer)
{
    transfer.append_child("key").text() = "Active";
    transfer.append_child(box.IsPaused ? "false" : "true");

    transfer.append_child("key").text() = "GroupValue";
    transfer.append_child("integer").text() = "-1";

    transfer.append_child("key").text() = "InternalTorrentPath";
    transfer.append_child("string").text() = torrentFilePath.string().c_str();

    transfer.append_child("key").text() = "RemoveWhenFinishedSeeding";
    transfer.append_child("false");

    transfer.append_child("key").text() = "TorrentHash";
    transfer.append_child("string").text() = box.Torrent.GetInfoHash().c_str();

    transfer.append_child("key").text() = "WaitToStart";
    transfer.append_child("false");
}

} // namespace

TransmissionStateStore::TransmissionStateStore(TransmissionStateType stateType) :
    m_stateType(stateType),
    m_bencoder(),
    m_tranfersPlistMutex()
{
    //
}

TransmissionStateStore::~TransmissionStateStore() = default;

TorrentClient::Enum TransmissionStateStore::GetTorrentClient() const
{
    return TorrentClient::Transmission;
}

fs::path TransmissionStateStore::GuessDataDir([[maybe_unused]] Intention::Enum intention) const
{
#if !defined(_WIN32)

    fs::path const homeDir = Util::GetEnvironmentVariable("HOME", {});
    if (homeDir.empty())
    {
        return {};
    }

#if defined(__APPLE__)

    fs::path const appSupportDir = homeDir / "Library" / "Application Support";

    fs::path const macDataDir = appSupportDir / Detail::MacDataDirName;
    if (!homeDir.empty() && IsValidDataDir(macDataDir, intention))
    {
        return macDataDir;
    }

#endif

    fs::path const xdgConfigHome = Util::GetEnvironmentVariable("XDG_CONFIG_HOME", {});
    fs::path const xdgConfigDir = !xdgConfigHome.empty() ? xdgConfigHome : homeDir / ".config";

    fs::path const commonDataDir = xdgConfigDir / Detail::CommonDataDirName;
    if (IsValidDataDir(commonDataDir, intention))
    {
        return commonDataDir;
    }

    fs::path const daemonDataDir = xdgConfigDir / Detail::DaemonDataDirName;
    if (IsValidDataDir(daemonDataDir, intention))
    {
        return daemonDataDir;
    }

#endif

    return {};
}

bool TransmissionStateStore::IsValidDataDir(fs::path const& dataDir, Intention::Enum intention) const
{
    if (intention == Intention::Import)
    {
        return fs::is_directory(dataDir);
    }

    return
        fs::is_directory(Detail::GetResumeDir(dataDir, m_stateType)) &&
        fs::is_directory(Detail::GetTorrentsDir(dataDir, m_stateType));
}

ITorrentStateIteratorPtr TransmissionStateStore::Export(fs::path const& /*dataDir*/,
    IFileStreamProvider const& /*fileStreamProvider*/) const
{
    throw NotImplementedException(__func__);
}

void TransmissionStateStore::Import(fs::path const& dataDir, Box const& box, IFileStreamProvider& fileStreamProvider) const
{
    namespace RField = Detail::ResumeField;

    if (box.BlockSize % Detail::BlockSize != 0)
    {
        // See trac #4005.
        throw ImportCancelledException(fmt::format("Transmission does not support torrents with piece length not multiple of two: {}",
            box.BlockSize));
    }

    for (Box::FileInfo const& file : box.Files)
    {
        if (!file.Path.is_relative())
        {
            throw ImportCancelledException(fmt::format("Transmission does not support moving files outside of download directory: {}",
                file.Path));
        }
    }

    ojson resume = ojson::object();

    //resume["activity-date"] = 0;
    resume[RField::AddedDate] = static_cast<std::int64_t>(box.AddedAt);
    //resume["bandwidth-priority"] = 0;
    resume[RField::Corrupt] = box.CorruptedSize;
    resume[RField::Destination] = box.SavePath.parent_path().string();
    resume[RField::Dnd] = ToStoreDoNotDownload(box.Files);
    resume[RField::DoneDate] = static_cast<std::int64_t>(box.CompletedAt);
    resume[RField::Downloaded] = box.DownloadedSize;
    //resume["downloading-time-seconds"] = 0;
    try {
    resume[RField::Files] = box.Torrent.GetFiles(box.SavePath.filename().string());
    } catch (...) {}
    //resume["idle-limit"] = ojson::object();
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
    resume[RField::Uploaded] = box.UploadedSize;

    Util::SortJsonObjectKeys(resume);

    TorrentInfo torrent = box.Torrent;
    torrent.SetTrackers(box.Trackers);

    std::string const baseName = Util::GetEnvironmentVariable("BT_MIGRATE_TRANSMISSION_2_9X", {}).empty() ?
        torrent.GetInfoHash() : box.Caption + '.' + torrent.GetInfoHash().substr(0, 16);

    fs::path const torrentFilePath = Detail::GetTorrentFilePath(dataDir, baseName, m_stateType);
    fs::create_directories(torrentFilePath.parent_path());

    fs::path const resumeFilePath = Detail::GetResumeFilePath(dataDir, baseName, m_stateType);
    fs::create_directories(resumeFilePath.parent_path());

    if (m_stateType == TransmissionStateType::Mac)
    {
        fs::path const transfersPlistPath = Detail::GetMacTransfersFilePath(dataDir);

        pugi::xml_document plistDoc;
        pugi::xml_node plistNode;
        pugi::xml_node arrayNode;

        // Avoid concurrent access to Transfers.plist, could lead to file corruption
        std::lock_guard<std::mutex> lock(m_tranfersPlistMutex);

        try
        {
            IReadStreamPtr const readStream = fileStreamProvider.GetReadStream(transfersPlistPath);
            plistDoc.load(*readStream, pugi::parse_default | pugi::parse_declaration | pugi::parse_doctype);
            plistNode = plistDoc.child("plist");
            arrayNode = plistNode.child("array");
        }
        catch (Exception const&)
        {
        }

        if (arrayNode.empty())
        {
            std::tie(plistDoc, plistNode) = CreateMacPropertyList();
            arrayNode = plistNode.append_child("array");
        }

        pugi::xml_node dictNode = arrayNode.append_child("dict");
        ToMacStoreTransfer(box, torrentFilePath, dictNode);

        IWriteStreamPtr const writeStream = fileStreamProvider.GetWriteStream(transfersPlistPath);
        plistDoc.save(*writeStream);
    }

    {
        IWriteStreamPtr const stream = fileStreamProvider.GetWriteStream(torrentFilePath);
        torrent.Encode(*stream, m_bencoder);
    }

    {
        IWriteStreamPtr const stream = fileStreamProvider.GetWriteStream(resumeFilePath);
        m_bencoder.Encode(*stream, resume);
    }
}
