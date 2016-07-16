#include "ImportHelper.h"

#include "Common/IFileStreamProvider.h"
#include "Common/IForwardIterator.h"
#include "Common/Logger.h"
#include "Common/SignalHandler.h"
#include "Store/DebugTorrentStateIterator.h"
#include "Store/ITorrentStateStore.h"
#include "Torrent/Box.h"

#include <boost/filesystem/path.hpp>

#include <exception>
#include <iostream>
#include <thread>
#include <vector>

namespace fs = boost::filesystem;

ImportHelper::Result::Result() :
    SuccessCount(0),
    FailCount(0),
    SkipCount(0)
{
    //
}

ImportHelper::ImportHelper(ITorrentStateStorePtr sourceStore, boost::filesystem::path const& sourceDataDir,
    ITorrentStateStorePtr targetStore, boost::filesystem::path const& targetDataDir, IFileStreamProvider& fileStreamProvider,
    SignalHandler const& signalHandler) :
    m_sourceStore(std::move(sourceStore)),
    m_sourceDataDir(sourceDataDir),
    m_targetStore(std::move(targetStore)),
    m_targetDataDir(targetDataDir),
    m_fileStreamProvider(fileStreamProvider),
    m_signalHandler(signalHandler)
{
    //
}

ImportHelper::~ImportHelper()
{
    //
}

ImportHelper::Result ImportHelper::Import(unsigned int threadCount)
{
    Result result;

    try
    {
        ITorrentStateIteratorPtr boxes = m_sourceStore->Export(m_sourceDataDir, m_fileStreamProvider);
        boxes = std::make_unique<DebugTorrentStateIterator>(std::move(boxes));

        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < threadCount; ++i)
        {
            threads.emplace_back(&ImportHelper::ImportImpl, this, std::cref(m_targetDataDir), std::ref(*boxes),
                std::ref(result));
        }

        for (auto& thread : threads)
        {
            thread.join();
        }
    }
    catch (std::exception const& e)
    {
        Logger(Logger::Error) << "Error: " << e.what();
        throw;
    }

    if (m_signalHandler.IsInterrupted())
    {
        throw Exception("Execution has been interrupted");
    }

    Logger(Logger::Info) << "Finished: " << result.SuccessCount << " succeeded, " << result.FailCount << " failed, " <<
        result.SkipCount << " skipped";

    return result;
}

void ImportHelper::ImportImpl(fs::path const& targetDataDir, ITorrentStateIterator& boxes, Result& result)
{
    Box box;
    while (!m_signalHandler.IsInterrupted() && boxes.GetNext(box))
    {
        std::string const prefix = "[" + box.SavePath.filename().string() + "] ";

        try
        {
            Logger(Logger::Info) << prefix << "Import started";
            m_targetStore->Import(targetDataDir, box, m_fileStreamProvider);
            ++result.SuccessCount;
            Logger(Logger::Info) << prefix << "Import succeeded";
        }
        catch (ImportCancelledException const& e)
        {
            ++result.SkipCount;
            Logger(Logger::Warning) << prefix << "Import skipped: " << e.what();
        }
        catch (std::exception const& e)
        {
            ++result.FailCount;
            Logger(Logger::Error) << prefix << "Import failed: " << e.what();
        }
    }
}
