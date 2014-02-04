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
