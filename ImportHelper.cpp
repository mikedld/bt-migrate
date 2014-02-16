#include "ImportHelper.h"

#include "Box.h"
#include "IFileStreamProvider.h"
#include "IForwardIterator.h"
#include "ITorrentStateStore.h"
#include "Logger.h"

#include <boost/filesystem/path.hpp>

#include <exception>
#include <iostream>

ImportHelper::ImportHelper()
{
    //
}

ImportHelper::~ImportHelper()
{
    //
}

void ImportHelper::Import(ITorrentStateStore& store, boost::filesystem::path const& dataDir,
    ITorrentStateIterator& boxes, IFileStreamProvider& fileStreamProvider)
{
    try
    {
        Box box;
        while (boxes.GetNext(box))
        {
            std::string const prefix = "[" + box.SavePath.filename().string() + "] ";

            try
            {
                Logger(Logger::Info) << prefix << "Import started";
                store.Import(dataDir, box, fileStreamProvider);
                Logger(Logger::Info) << prefix << "Import finished";
            }
            catch (std::exception const& e)
            {
                Logger(Logger::Warning) << prefix << "Import failed: " << e.what();
            }
        }
    }
    catch (std::exception const& e)
    {
        Logger(Logger::Error) << "Error: " << e.what();
    }
}
