#include "DebugTorrentStateIterator.h"

#include "Box.h"

#include <iostream>

std::ostream& operator << (std::ostream& stream, Box::LimitInfo const& value)
{
    switch (value.Mode)
    {
    case Box::LimitMode::Inherit:
        stream << "Inherit";
        break;
    case Box::LimitMode::Enabled:
        stream << "Enabled";
        break;
    case Box::LimitMode::Disabled:
        stream << "Disabled";
        break;
    }

    stream << " / " << value.Value;

    return stream;
}

std::ostream& operator << (std::ostream& stream, Box::FileInfo const& value)
{
    stream <<
        std::boolalpha << value.DoNotDownload << " / " <<
        value.Priority;

    return stream;
}

std::ostream& operator << (std::ostream& stream, std::vector<bool> const& value)
{
    for (bool const x : value)
    {
        stream << (x ? '#' : '-');
    }

    return stream;
}

DebugTorrentStateIterator::DebugTorrentStateIterator(ITorrentStateIteratorPtr decoratee) :
    m_decoratee(std::move(decoratee))
{
    //
}

DebugTorrentStateIterator::~DebugTorrentStateIterator()
{
    //
}

bool DebugTorrentStateIterator::GetNext(Box& nextBox)
{
    if (!m_decoratee->GetNext(nextBox))
    {
        return false;
    }

    std::cout << "---" << std::endl;

    std::cout <<
        "InfoHash = \"" << nextBox.InfoHash << "\"" << std::endl <<
        // "Torrent = " << nextBox.Torrent << std::endl <<
        "AddedAt = " << nextBox.AddedAt << std::endl <<
        "CompletedAt = " << nextBox.CompletedAt << std::endl <<
        "IsPaused = " << std::boolalpha << nextBox.IsPaused << std::endl <<
        "DownloadedSize = " << nextBox.DownloadedSize << std::endl <<
        "UploadedSize = " << nextBox.UploadedSize << std::endl <<
        "CorruptedSize = " << nextBox.CorruptedSize << std::endl <<
        "SavePath = \"" << nextBox.SavePath << "\"" << std::endl <<
        "BlockSize = " << nextBox.BlockSize << std::endl <<
        "RatioLimit = " << nextBox.RatioLimit << std::endl <<
        "DownloadSpeedLimit = " << nextBox.DownloadSpeedLimit << std::endl <<
        "UploadSpeedLimit = " << nextBox.UploadSpeedLimit << std::endl;

    std::cout << "Files =" << std::endl;
    for (Box::FileInfo const& file : nextBox.Files)
    {
        std::cout << "  " << file << std::endl;
    }

    std::cout << "ValidBlocks = " << nextBox.ValidBlocks << std::endl;

    return true;
}
