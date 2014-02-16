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

#include "uTorrentStateStore.h"

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

#include <json/value.h>
#include <json/writer.h>

#include <iostream>
#include <mutex>

namespace fs = boost::filesystem;

namespace Detail
{

namespace ResumeField
{

std::string const AddedOn = "added_on";
std::string const CompletedOn = "completed_on";
std::string const Corrupt = "corrupt";
std::string const Downloaded = "downloaded";
std::string const DownSpeed = "downspeed";
std::string const Have = "have";
std::string const OverrideSeedSettings = "override_seedsettings";
std::string const Path = "path";
std::string const Prio = "prio";
std::string const Started = "started";
std::string const Targets = "targets";
std::string const Uploaded = "uploaded";
std::string const UpSpeed = "upspeed";
std::string const WantedRatio = "wanted_ratio";

} // namespace ResumeField

enum Priority
{
    DoNotDownloadPriority = 0,
    MinPriority = 4,
    MaxPriority = 12
};

enum TorrentState
{
    StoppedState = 0,
    StartedState = 2,
    PausedState = 3
};

std::string const ResumeFilename = "resume.dat";

} // namespace Detail

namespace
{

typedef std::unique_ptr<Json::Value> JsonValuePtr;

Box::LimitInfo FromStoreRatioLimit(Json::Value const& enabled, Json::Value const& storeLimit)
{
    Box::LimitInfo result;
    result.Mode = enabled.asInt() != 0 ? Box::LimitMode::Enabled : Box::LimitMode::Inherit;
    result.Value = storeLimit.asDouble() / 1000.;
    return result;
}

Box::LimitInfo FromStoreSpeedLimit(Json::Value const& storeLimit)
{
    Box::LimitInfo result;
    result.Mode = storeLimit.asInt() > 0 ? Box::LimitMode::Enabled : Box::LimitMode::Inherit;
    result.Value = storeLimit.asInt();
    return result;
}

fs::path GetChangedFilePath(Json::Value const& targets, std::size_t index)
{
    fs::path result;

    if (!targets.isNull())
    {
        for (Json::Value const& target : targets)
        {
            if (target[0].asUInt() == index)
            {
                result = Util::GetPath(target[1].asString());
                break;
            }
        }
    }

    return result;
}

class uTorrentTorrentStateIterator : public ITorrentStateIterator
{
public:
    uTorrentTorrentStateIterator(fs::path const& dataDir, JsonValuePtr resume, IFileStreamProvider& fileStreamProvider);

public:
    // ITorrentStateIterator
    virtual bool GetNext(Box& nextBox);

private:
    fs::path const m_dataDir;
    JsonValuePtr const m_resume;
    IFileStreamProvider& m_fileStreamProvider;
    Json::Value::iterator m_torrentIt;
    Json::Value::iterator const m_torrentEnd;
    std::mutex m_torrentItMutex;
    BencodeCodec const m_bencoder;
};

uTorrentTorrentStateIterator::uTorrentTorrentStateIterator(fs::path const& dataDir, JsonValuePtr resume,
    IFileStreamProvider& fileStreamProvider) :
    m_dataDir(dataDir),
    m_resume(std::move(resume)),
    m_fileStreamProvider(fileStreamProvider),
    m_torrentIt(m_resume->begin()),
    m_torrentEnd(m_resume->end()),
    m_torrentItMutex(),
    m_bencoder()
{
    //
}

bool uTorrentTorrentStateIterator::GetNext(Box& nextBox)
{
    namespace RField = Detail::ResumeField;

    std::unique_lock<std::mutex> lock(m_torrentItMutex);

    fs::path torrentFilename;
    while (m_torrentIt != m_torrentEnd)
    {
        static std::string const TorrentFileExtension = ".torrent";

        fs::path key = m_torrentIt.key().asString();
        if (key.extension() == TorrentFileExtension)
        {
            torrentFilename = std::move(key);
            break;
        }

        ++m_torrentIt;
    }

    if (torrentFilename.empty())
    {
        return false;
    }

    Json::Value const& resume = *m_torrentIt++;

    lock.unlock();

    Box box;

    {
        ReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(m_dataDir / torrentFilename);
        box.Torrent = TorrentInfo::FromStream(*stream, m_bencoder);
    }

    box.AddedAt = resume[RField::AddedOn].asInt();
    box.CompletedAt = resume[RField::CompletedOn].asInt();
    box.IsPaused = resume[RField::Started].asInt() == Detail::PausedState ||
        resume[RField::Started].asInt() == Detail::StoppedState;
    box.DownloadedSize = resume[RField::Downloaded].asUInt64();
    box.UploadedSize = resume[RField::Uploaded].asUInt64();
    box.CorruptedSize = resume[RField::Corrupt].asUInt64();
    box.SavePath = Util::GetPath(resume[RField::Path].asString());
    box.BlockSize = box.Torrent.GetPieceSize();
    box.RatioLimit = FromStoreRatioLimit(resume[RField::OverrideSeedSettings], resume[RField::WantedRatio]);
    box.DownloadSpeedLimit = FromStoreSpeedLimit(resume[RField::DownSpeed]);
    box.UploadSpeedLimit = FromStoreSpeedLimit(resume[RField::UpSpeed]);

    std::string const filePriorities = resume[RField::Prio].asString();
    Json::Value const& targets = resume[RField::Targets];
    box.Files.reserve(filePriorities.size());
    for (std::size_t i = 0; i < filePriorities.size(); ++i)
    {
        int const filePriority = filePriorities[i];
        fs::path const changedPath = GetChangedFilePath(targets, i);

        Box::FileInfo file;
        file.DoNotDownload = filePriority == Detail::DoNotDownloadPriority;
        file.Priority = file.DoNotDownload ? Box::NormalPriority : BoxHelper::Priority::FromStore(filePriority,
            Detail::MinPriority, Detail::MaxPriority);
        file.Path = changedPath;
        box.Files.push_back(std::move(file));
    }

    std::uint64_t const totalSize = box.Torrent.GetTotalSize();
    std::uint64_t const totalBlockCount = (totalSize + box.BlockSize - 1) / box.BlockSize;
    box.ValidBlocks.reserve(totalBlockCount + 8);
    for (unsigned char const c : resume[RField::Have].asString())
    {
        for (int i = 0; i < 8; ++i)
        {
            bool const isPieceValid = (c & (1 << i)) != 0;
            box.ValidBlocks.push_back(isPieceValid);
        }
    }

    box.ValidBlocks.resize(totalBlockCount);

    nextBox = std::move(box);
    return true;
}

} // namespace

uTorrentStateStore::uTorrentStateStore()
{
    //
}

uTorrentStateStore::~uTorrentStateStore()
{
    //
}

TorrentClient::Enum uTorrentStateStore::GetTorrentClient() const
{
    return TorrentClient::uTorrent;
}

fs::path uTorrentStateStore::GuessDataDir(Intention::Enum /*intention*/) const
{
    throw NotImplementedException(__func__);
}

bool uTorrentStateStore::IsValidDataDir(fs::path const& dataDir, Intention::Enum /*intention*/) const
{
    return fs::is_regular_file(dataDir / Detail::ResumeFilename);
}

ITorrentStateIteratorPtr uTorrentStateStore::Export(fs::path const& dataDir, IFileStreamProvider& fileStreamProvider) const
{
    JsonValuePtr resume(new Json::Value());
    {
        ReadStreamPtr const stream = fileStreamProvider.GetReadStream(dataDir / Detail::ResumeFilename);
        BencodeCodec().Decode(*stream, *resume);
    }

    return ITorrentStateIteratorPtr(new uTorrentTorrentStateIterator(dataDir, std::move(resume), fileStreamProvider));
}

void uTorrentStateStore::Import(fs::path const& /*dataDir*/, Box const& /*box*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    throw NotImplementedException(__func__);
}
