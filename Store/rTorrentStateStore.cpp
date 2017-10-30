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

#include "rTorrentStateStore.h"

#include "Codec/BencodeCodec.h"
#include "Common/Exception.h"
#include "Common/IFileStreamProvider.h"
#include "Common/IForwardIterator.h"
#include "Common/Throw.h"
#include "Common/Util.h"
#include "Torrent/Box.h"
#include "Torrent/BoxHelper.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <jsoncons/json.hpp>

#include <mutex>

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

namespace Detail
{

namespace ResumeField
{

std::string const Bitfield = "bitfield";
std::string const Files = "files";
std::string const Trackers = "trackers";

namespace FileField
{

std::string const Priority = "priority";

} // namespace FileField

namespace TrackerField
{

std::string const Enabled = "enabled";

} // namespace TrackerField

} // namespace ResumeField

namespace StateField
{

std::string const Directory = "directory";
std::string const Priority = "priority";
std::string const TimestampFinished = "timestamp.finished";
std::string const TimestampStarted = "timestamp.started";
std::string const TotalUploaded = "total_uploaded";

} // namespace StateField

enum Priority
{
    DoNotDownloadPriority = 0,
    MinPriority = -1,
    MaxPriority = 1
};

std::string const ConfigFilename = ".rtorrent.rc";
std::string const StateFileExtension = ".rtorrent";
std::string const LibTorrentStateFileExtension = ".libtorrent_resume";

} // namespace Detail

namespace
{

class rTorrentTorrentStateIterator : public ITorrentStateIterator
{
public:
    rTorrentTorrentStateIterator(fs::path const& dataDir, IFileStreamProvider const& fileStreamProvider);

public:
    // ITorrentStateIterator
    virtual bool GetNext(Box& nextBox);

private:
    fs::path const m_dataDir;
    IFileStreamProvider const& m_fileStreamProvider;
    fs::directory_iterator m_directoryIt;
    fs::directory_iterator const m_directoryEnd;
    std::mutex m_directoryItMutex;
    BencodeCodec const m_bencoder;
};


rTorrentTorrentStateIterator::rTorrentTorrentStateIterator(fs::path const& dataDir, IFileStreamProvider const& fileStreamProvider) :
    m_dataDir(dataDir),
    m_fileStreamProvider(fileStreamProvider),
    m_directoryIt(m_dataDir),
    m_directoryEnd(),
    m_directoryItMutex(),
    m_bencoder()
{
    //
}

bool rTorrentTorrentStateIterator::GetNext(Box& nextBox)
{
    namespace RField = Detail::ResumeField;
    namespace SField = Detail::StateField;

    std::unique_lock<std::mutex> lock(m_directoryItMutex);

    fs::path stateFilePath;
    while (m_directoryIt != m_directoryEnd)
    {
        fs::path const& path = m_directoryIt->path();
        if (path.extension() == Detail::StateFileExtension && m_directoryIt->status().type() == fs::regular_file)
        {
            stateFilePath = path;
            break;
        }

        ++m_directoryIt;
    }

    if (stateFilePath.empty())
    {
        return false;
    }

    ++m_directoryIt;

    lock.unlock();

    fs::path libTorrentStateFilePath = stateFilePath;
    libTorrentStateFilePath.replace_extension(Detail::LibTorrentStateFileExtension);

    Box box;

    {
        fs::path torrentFilePath = stateFilePath;
        torrentFilePath.replace_extension(fs::path());

        IReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(torrentFilePath);
        box.Torrent = TorrentInfo::Decode(*stream, m_bencoder);

        std::string const infoHashFromFilename = torrentFilePath.stem().string();
        if (!boost::algorithm::iequals(box.Torrent.GetInfoHash(), infoHashFromFilename))
        {
            Throw<Exception>() << "Info hashes don't match: " << box.Torrent.GetInfoHash() << " vs. " << infoHashFromFilename;
        }
    }

    ojson state;
    {
        IReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(stateFilePath);
        m_bencoder.Decode(*stream, state);
    }

    ojson resume;
    {
        IReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(libTorrentStateFilePath);
        m_bencoder.Decode(*stream, resume);
    }

    box.AddedAt = state[SField::TimestampStarted].as_integer();
    box.CompletedAt = state[SField::TimestampFinished].as_integer();
    box.IsPaused = state[SField::Priority].as_integer() == 0;
    box.UploadedSize = state[SField::TotalUploaded].as_uinteger();
    box.SavePath = Util::GetPath(state[SField::Directory].as_string());
    box.BlockSize = box.Torrent.GetPieceSize();

    box.Files.reserve(resume[RField::Files].size());
    for (ojson const& file : resume[RField::Files].array_range())
    {
        namespace ff = RField::FileField;

        int const filePriority = file[ff::Priority].as_integer();

        Box::FileInfo boxFile;
        boxFile.DoNotDownload = filePriority == Detail::DoNotDownloadPriority;
        boxFile.Priority = boxFile.DoNotDownload ? Box::NormalPriority : BoxHelper::Priority::FromStore(filePriority - 1,
            Detail::MinPriority, Detail::MaxPriority);
        box.Files.push_back(std::move(boxFile));
    }

    std::uint64_t const totalSize = box.Torrent.GetTotalSize();
    std::uint64_t const totalBlockCount = (totalSize + box.BlockSize - 1) / box.BlockSize;
    box.ValidBlocks.reserve(totalBlockCount + 8);
    for (unsigned char const c : resume[RField::Bitfield].as_string())
    {
        for (int i = 7; i >= 0; --i)
        {
            bool const isPieceValid = (c & (1 << i)) != 0;
            box.ValidBlocks.push_back(isPieceValid);
        }
    }

    box.ValidBlocks.resize(totalBlockCount);

    for (auto const& tracker : resume[RField::Trackers].object_range())
    {
        namespace tf = RField::TrackerField;

        std::string const url = tracker.key();
        if (url == "dht://")
        {
            continue;
        }

        ojson const& params = tracker.value();
        if (params[tf::Enabled].as_integer() == 1)
        {
            box.Trackers.push_back({url});
        }
    }

    nextBox = std::move(box);
    return true;
}

} // namespace

rTorrentStateStore::rTorrentStateStore()
{
    //
}

rTorrentStateStore::~rTorrentStateStore()
{
    //
}

TorrentClient::Enum rTorrentStateStore::GetTorrentClient() const
{
    return TorrentClient::rTorrent;
}

fs::path rTorrentStateStore::GuessDataDir(Intention::Enum intention) const
{
#ifndef _WIN32

    fs::path const homeDir = std::getenv("HOME");

    if (!fs::is_regular_file(homeDir / Detail::ConfigFilename))
    {
        return fs::path();
    }

    pt::ptree config;
    {
        fs::ifstream stream(homeDir / Detail::ConfigFilename, std::ios_base::in);
        pt::ini_parser::read_ini(stream, config);
    }

    fs::path const dataDirPath = Util::GetPath(config.get<std::string>("session"));
    if (!IsValidDataDir(dataDirPath, intention))
    {
        return fs::path();
    }

    return dataDirPath;

#else

    throw NotImplementedException(__func__);

#endif
}

bool rTorrentStateStore::IsValidDataDir(fs::path const& dataDir, Intention::Enum intention) const
{
    if (intention == Intention::Import)
    {
        return fs::is_directory(dataDir);
    }

    for (fs::directory_iterator it(dataDir), end; it != end; ++it)
    {
        fs::path path = it->path();
        if (path.extension() != Detail::StateFileExtension || it->status().type() != fs::regular_file)
        {
            continue;
        }

        if (!fs::is_regular_file(path.replace_extension(Detail::LibTorrentStateFileExtension)))
        {
            continue;
        }

        if (!fs::is_regular_file(path.replace_extension(fs::path())))
        {
            continue;
        }

        return true;
    }

    return false;
}

ITorrentStateIteratorPtr rTorrentStateStore::Export(fs::path const& dataDir, IFileStreamProvider const& fileStreamProvider) const
{
    return std::make_unique<rTorrentTorrentStateIterator>(dataDir, fileStreamProvider);
}

void rTorrentStateStore::Import(fs::path const& /*dataDir*/, Box const& /*box*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    throw NotImplementedException(__func__);
}
