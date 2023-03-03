#pragma once

#include <filesystem>
#include <memory>

template<typename... ArgsT>
class IForwardIterator;

struct Box;
typedef IForwardIterator<Box> ITorrentStateIterator;
typedef std::unique_ptr<ITorrentStateIterator> ITorrentStateIteratorPtr;

class IFileStreamProvider;

class ITorrentStateStore;
typedef std::unique_ptr<ITorrentStateStore> ITorrentStateStorePtr;

class SignalHandler;

class ImportHelper
{
public:
    struct Result
    {
        std::size_t SuccessCount;
        std::size_t FailCount;
        std::size_t SkipCount;

        Result();
    };

public:
    ImportHelper(ITorrentStateStorePtr sourceStore, std::filesystem::path const& sourceDataDir,
        ITorrentStateStorePtr targetStore, std::filesystem::path const& targetDataDir,
        IFileStreamProvider& fileStreamProvider, SignalHandler const& signalHandler);
    ~ImportHelper();

    Result Import(unsigned int threadCount);

private:
    void ImportImpl(std::filesystem::path const& targetDataDir, ITorrentStateIterator& boxes, Result& result);

private:
    ITorrentStateStorePtr const m_sourceStore;
    std::filesystem::path const m_sourceDataDir;
    ITorrentStateStorePtr const m_targetStore;
    std::filesystem::path const m_targetDataDir;
    IFileStreamProvider& m_fileStreamProvider;
    SignalHandler const& m_signalHandler;
};
