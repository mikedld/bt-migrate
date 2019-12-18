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

#include "DebugTorrentStateIterator.h"

#include "Torrent/Box.h"

template<typename StreamT>
StreamT& operator << (StreamT& stream, TorrentInfo const& value)
{
    stream << "(" << value.GetInfoHash() << ")";
    return stream;
}

template<typename StreamT>
StreamT& operator << (StreamT& stream, Box::LimitInfo const& value)
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

    stream << "/" << value.Value;

    return stream;
}

template<typename StreamT>
StreamT& operator << (StreamT& stream, Box::FileInfo const& value)
{
    stream <<
        "(" <<
        std::boolalpha << value.DoNotDownload << "/" <<
        value.Priority << "/" <<
        value.Path <<
        ")";

    return stream;
}

template<typename StreamT, typename T>
StreamT& operator << (StreamT& stream, std::vector<T> const& value)
{
    stream << '[';

    bool first = true;
    for (T const& x : value)
    {
        if (!first)
        {
            stream << ", ";
        }

        stream << x;
        first = false;
    }

    stream << ']';
    return stream;
}

template<typename StreamT>
StreamT& operator << (StreamT& stream, std::vector<bool> const& value)
{
    for (bool const x : value)
    {
        stream << (x ? '#' : '-');
    }

    return stream;
}

// Including after operator declarations so that lookup works
#include "Common/Logger.h"

DebugTorrentStateIterator::DebugTorrentStateIterator(ITorrentStateIteratorPtr decoratee) :
    m_decoratee(std::move(decoratee))
{
    //
}

DebugTorrentStateIterator::~DebugTorrentStateIterator() = default;

bool DebugTorrentStateIterator::GetNext(Box& nextBox)
{
    if (!m_decoratee->GetNext(nextBox))
    {
        return false;
    }

    Logger(Logger::Debug) <<
        "Torrent=" << nextBox.Torrent << " "
        "AddedAt=" << nextBox.AddedAt << " "
        "CompletedAt=" << nextBox.CompletedAt << " "
        "IsPaused=" << std::boolalpha << nextBox.IsPaused << " "
        "DownloadedSize=" << nextBox.DownloadedSize << " "
        "UploadedSize=" << nextBox.UploadedSize << " "
        "CorruptedSize=" << nextBox.CorruptedSize << " "
        "SavePath=" << nextBox.SavePath << " "
        "BlockSize=" << nextBox.BlockSize << " "
        "RatioLimit=" << nextBox.RatioLimit << " "
        "DownloadSpeedLimit=" << nextBox.DownloadSpeedLimit << " "
        "UploadSpeedLimit=" << nextBox.UploadSpeedLimit << " "
        "Files<" << nextBox.Files.size() << ">=" << nextBox.Files << " "
        "ValidBlocks<" << nextBox.ValidBlocks.size() << ">=" << nextBox.ValidBlocks << " "
        "Trackers<" << nextBox.Trackers.size() << ">=" << nextBox.Trackers;

    return true;
}
