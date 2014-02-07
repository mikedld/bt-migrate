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

#include "BencodeCodec.h"
#include "Box.h"
#include "BoxHelper.h"
#include "Exception.h"
#include "IFileStreamProvider.h"
#include "IForwardIterator.h"
#include "Throw.h"
#include "Util.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <mutex>

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

namespace rTorrent
{

enum Priority
{
    DoNotDownloadPriority = 0,
    MinPriority = -1,
    MaxPriority = 1
};

std::string const ConfigFilename = ".rtorrent.rc";
std::string const StateFileExtension = ".rtorrent";
std::string const LibTorrentStateFileExtension = ".libtorrent_resume";

} // namespace rTorrent

namespace
{

class rTorrentTorrentStateIterator : public ITorrentStateIterator
{
public:
    rTorrentTorrentStateIterator(fs::path const& dataDir, IFileStreamProvider& fileStreamProvider);

public:
    // ITorrentStateIterator
    virtual bool GetNext(Box& nextBox);

private:
    fs::path const m_dataDir;
    IFileStreamProvider& m_fileStreamProvider;
    fs::directory_iterator m_directoryIt;
    fs::directory_iterator const m_directoryEnd;
    std::mutex m_directoryItMutex;
    BencodeCodec const m_bencoder;
};


rTorrentTorrentStateIterator::rTorrentTorrentStateIterator(fs::path const& dataDir, IFileStreamProvider& fileStreamProvider) :
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
    std::unique_lock<std::mutex> lock(m_directoryItMutex);

    fs::path stateFilePath;
    while (m_directoryIt != m_directoryEnd)
    {
        fs::path const& path = m_directoryIt->path();
        if (path.extension() == rTorrent::StateFileExtension && m_directoryIt->status().type() == fs::regular_file)
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
    libTorrentStateFilePath.replace_extension(rTorrent::LibTorrentStateFileExtension);

    Box box;

    {
        fs::path torrentFilePath = stateFilePath;
        torrentFilePath.replace_extension(fs::path());

        ReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(torrentFilePath);
        BoxHelper::LoadTorrent(*stream, box);

        std::string const infoHashFromFilename = torrentFilePath.stem().string();
        if (!boost::algorithm::iequals(box.InfoHash, infoHashFromFilename))
        {
            Throw<Exception>() << "Info hashes don't match: " << box.InfoHash << " vs. " << infoHashFromFilename;
        }
    }

    Json::Value state;
    {
        ReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(stateFilePath);
        m_bencoder.Decode(*stream, state);
    }

    Json::Value resume;
    {
        ReadStreamPtr const stream = m_fileStreamProvider.GetReadStream(libTorrentStateFilePath);
        m_bencoder.Decode(*stream, resume);
    }

    box.AddedAt = state["timestamp.started"].asInt();
    box.CompletedAt = state["timestamp.finished"].asInt();
    box.IsPaused = state["priority"].asInt() == 0;
    box.UploadedSize = state["total_uploaded"].asUInt64();
    box.SavePath = Util::GetPath(state["directory"].asString()).parent_path().string();
    box.BlockSize = box.Torrent["info"]["piece length"].asUInt();

    box.Files.reserve(resume["files"].size());
    for (Json::Value const& file : resume["files"])
    {
        int const filePriority = file["priority"].asInt();

        Box::FileInfo boxFile;
        boxFile.DoNotDownload = filePriority == rTorrent::DoNotDownloadPriority;
        boxFile.Priority = boxFile.DoNotDownload ? Box::NormalPriority : BoxHelper::Priority::FromStore(filePriority - 1,
            rTorrent::MinPriority, rTorrent::MaxPriority);
        box.Files.push_back(std::move(boxFile));
    }

    std::uint64_t const totalSize = Util::GetTotalTorrentSize(box.Torrent);
    std::uint64_t const totalBlockCount = (totalSize + box.BlockSize - 1) / box.BlockSize;
    box.ValidBlocks.reserve(totalBlockCount + 8);
    for (unsigned char const c : resume["bitfield"].asString())
    {
        for (int i = 7; i >= 0; --i)
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

    if (!fs::is_regular_file(homeDir / rTorrent::ConfigFilename))
    {
        return fs::path();
    }

    pt::ptree config;
    {
        fs::ifstream stream(homeDir / rTorrent::ConfigFilename, std::ios_base::in);
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
        if (path.extension() != rTorrent::StateFileExtension || it->status().type() != fs::regular_file)
        {
            continue;
        }

        if (!fs::is_regular_file(path.replace_extension(rTorrent::LibTorrentStateFileExtension)))
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

ITorrentStateIteratorPtr rTorrentStateStore::Export(fs::path const& dataDir, IFileStreamProvider& fileStreamProvider) const
{
    if (!IsValidDataDir(dataDir, Intention::Export))
    {
        Throw<Exception>() << "Bad rTorrent data directory: " << dataDir;
    }

    return ITorrentStateIteratorPtr(new rTorrentTorrentStateIterator(dataDir, fileStreamProvider));
}

void rTorrentStateStore::Import(fs::path const& dataDir, ITorrentStateIteratorPtr /*boxes*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    if (!IsValidDataDir(dataDir, Intention::Import))
    {
        Throw<Exception>() << "Bad rTorrent data directory: " << dataDir;
    }

    throw NotImplementedException(__func__);
}
