// bt-migrate, torrent state migration tool
// Copyright (C) 2019 Mike Gelfand <mikedld@mikedld.com>
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

#include "uTorrentWebStateStore.h"

#include "Codec/BencodeCodec.h"
#include "Common/Exception.h"
#include "Common/IFileStreamProvider.h"
#include "Common/IForwardIterator.h"
#include "Common/Logger.h"
#include "Common/Throw.h"
#include "Common/Util.h"
#include "Torrent/Box.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <jsoncons/json.hpp>

#include <sqlite_orm/sqlite_orm.h>

#include <mutex>
#include <sstream>

namespace fs = boost::filesystem;

namespace
{
namespace Detail
{

struct ResumeInfo
{
    std::string InfoHash;
    std::vector<char> ResumeData;
    std::unique_ptr<std::string> SavePath;
};

namespace ResumeField
{

std::string const AddedTime = "added_time";
std::string const CompletedTime = "completed_time";
std::string const Info = "info";
std::string const Paused = "paused";
std::string const Pieces = "pieces";
std::string const SavePath = "save_path";
std::string const TotalDownloaded = "total_downloaded";
std::string const TotalUploaded = "total_uploaded";
std::string const Trackers = "trackers";
std::string const UrlList = "url-list";

} // namespace ResumeField

namespace TorrentField
{

std::string const Info = "info";
std::string const UrlList = "url-list";

} // namespace TorrentField

std::string const DataDirName = "uTorrent Web";
std::string const ResumeFilename = "resume.dat";
std::string const StoreFilename = "store.dat";

} // namespace Detail
} // namespace

namespace
{

auto OpenResumeDatabase(fs::path const& path)
{
    using namespace sqlite_orm;

    return make_storage(path.string(),
        make_table("TORRENTS",
            make_column("INFOHASH", &Detail::ResumeInfo::InfoHash, primary_key()),
            make_column("RESUME", &Detail::ResumeInfo::ResumeData),
            make_column("SAVE_PATH", &Detail::ResumeInfo::SavePath)));
}

class uTorrentWebTorrentStateIterator : public ITorrentStateIterator
{
    using ResumeDatabase = decltype(OpenResumeDatabase({}));
    using ResumeInfoEnumerator = std::invoke_result_t<decltype(&ResumeDatabase::iterate<Detail::ResumeInfo>), ResumeDatabase>;
    using ResumeInfoIterator = std::invoke_result_t<decltype(&ResumeInfoEnumerator::begin), ResumeInfoEnumerator>;

public:
    uTorrentWebTorrentStateIterator(fs::path const& stateDir, ResumeDatabase&& resumeDb,
        IFileStreamProvider const& fileStreamProvider);

public:
    // ITorrentStateIterator
    bool GetNext(Box& nextBox) override;

private:
    bool GetNext(Detail::ResumeInfo& resumeInfo);

private:
    fs::path const m_stateDir;
    ResumeDatabase m_resumeDb;
    IFileStreamProvider const& m_fileStreamProvider;
    ResumeInfoEnumerator m_resumeInfoEnumerator;
    ResumeInfoIterator m_resumeInfoIt;
    ResumeInfoIterator m_resumeInfoEnd;
    std::mutex m_resumeItMutex;
    BencodeCodec const m_bencoder;
};

uTorrentWebTorrentStateIterator::uTorrentWebTorrentStateIterator(fs::path const& stateDir, ResumeDatabase&& resumeDb,
    IFileStreamProvider const& fileStreamProvider) :
    m_stateDir(stateDir),
    m_resumeDb(resumeDb),
    m_fileStreamProvider(fileStreamProvider),
    m_resumeInfoEnumerator(m_resumeDb.iterate<Detail::ResumeInfo>()),
    m_resumeInfoIt(m_resumeInfoEnumerator.begin()),
    m_resumeInfoEnd(m_resumeInfoEnumerator.end()),
    m_resumeItMutex()
{
    //
}

bool uTorrentWebTorrentStateIterator::GetNext(Box& nextBox)
{
    namespace RField = Detail::ResumeField;
    namespace TField = Detail::TorrentField;

    Detail::ResumeInfo resumeInfo;
    if (!GetNext(resumeInfo))
    {
        return false;
    }

    ojson resume;
    {
        std::string resumeData{resumeInfo.ResumeData.begin(), resumeInfo.ResumeData.end()};
        std::istringstream stream(resumeData, std::ios_base::in | std::ios_base::binary);
        m_bencoder.Decode(stream, resume);
    }

    Box box;

    box.Torrent = ojson{ojson::object{
        {TField::Info, resume[RField::Info]},
        {TField::UrlList, resume.get_with_default(RField::UrlList, ojson{ojson::array{}})}
    }};

    box.AddedAt = resume[RField::AddedTime].as<std::time_t>();
    box.CompletedAt = resume[RField::CompletedTime].as<std::time_t>();
    box.IsPaused = resume[RField::Paused].as<bool>();
    box.DownloadedSize = resume[RField::TotalDownloaded].as<std::uint64_t>();
    box.UploadedSize = resume[RField::TotalUploaded].as<std::uint64_t>();
    box.CorruptedSize = 0;
    box.SavePath = Util::GetPath(resume[RField::SavePath].as_string()) / box.Torrent.GetName();
    box.BlockSize = box.Torrent.GetPieceSize();

    std::uint64_t const totalSize = box.Torrent.GetTotalSize();
    std::uint64_t const totalBlockCount = (totalSize + box.BlockSize - 1) / box.BlockSize;
    box.ValidBlocks.reserve(totalBlockCount);
    for (bool const isPieceValid : resume[RField::Pieces].as<std::string>())
    {
        box.ValidBlocks.push_back(isPieceValid);
    }

    box.Trackers = resume.get_with_default(RField::Trackers, ojson{ojson::array{}}).as<decltype(box.Trackers)>();

    nextBox = std::move(box);
    return true;
}

bool uTorrentWebTorrentStateIterator::GetNext(Detail::ResumeInfo& resumeInfo)
{
    std::lock_guard<std::mutex> lock(m_resumeItMutex);

    if (m_resumeInfoIt == m_resumeInfoEnd)
    {
        return false;
    }

    resumeInfo = std::move(*m_resumeInfoIt);

    ++m_resumeInfoIt;
    return true;
}

} // namespace

uTorrentWebStateStore::uTorrentWebStateStore() = default;
uTorrentWebStateStore::~uTorrentWebStateStore() = default;

TorrentClient::Enum uTorrentWebStateStore::GetTorrentClient() const
{
    return TorrentClient::uTorrentWeb;
}

fs::path uTorrentWebStateStore::GuessDataDir(Intention::Enum intention) const
{
#ifdef _WIN32

    fs::path const appDataDir = Util::GetEnvironmentVariable("APPDATA", {});

    if (IsValidDataDir(appDataDir / Detail::DataDirName, intention))
    {
        return appDataDir / Detail::DataDirName;
    }

#endif

    return {};
}

bool uTorrentWebStateStore::IsValidDataDir(fs::path const& dataDir, Intention::Enum /*intention*/) const
{
    return
        fs::is_regular_file(dataDir / Detail::ResumeFilename) &&
        fs::is_regular_file(dataDir / Detail::StoreFilename);
}

ITorrentStateIteratorPtr uTorrentWebStateStore::Export(fs::path const& dataDir, IFileStreamProvider const& fileStreamProvider) const
{
    Logger(Logger::Debug) << "[uTorrentWeb] Loading " << Detail::ResumeFilename;

    auto resumeDb = OpenResumeDatabase(dataDir / Detail::ResumeFilename);

    return std::make_unique<uTorrentWebTorrentStateIterator>(dataDir, std::move(resumeDb), fileStreamProvider);
}

void uTorrentWebStateStore::Import(fs::path const& /*dataDir*/, Box const& /*box*/,
    IFileStreamProvider& /*fileStreamProvider*/) const
{
    throw NotImplementedException(__func__);
}
