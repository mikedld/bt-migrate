#pragma once

#include <memory>

namespace boost { namespace filesystem { class path; } }

template<typename... ArgsT>
class IForwardIterator;

struct Box;
typedef IForwardIterator<Box> ITorrentStateIterator;
typedef std::unique_ptr<ITorrentStateIterator> ITorrentStateIteratorPtr;

class IFileStreamProvider;
class ITorrentStateStore;

class ImportHelper
{
public:
    ImportHelper();
    ~ImportHelper();

    void Import(ITorrentStateStore& store, boost::filesystem::path const& dataDir, ITorrentStateIterator& boxes,
        IFileStreamProvider& fileStreamProvider);
};
