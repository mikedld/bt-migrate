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

#pragma once

#include "TorrentInfo.h"

#include <boost/filesystem/path.hpp>

#include <cstdint>
#include <ctime>
#include <vector>

struct Box
{
    enum Priority
    {
        MinPriority = -20,
        NormalPriority = 0,
        MaxPriority = 20
    };

    enum struct LimitMode
    {
        Inherit,
        Enabled,
        Disabled
    };

    struct LimitInfo
    {
        LimitMode Mode;
        double Value;

        LimitInfo();
    };

    struct FileInfo
    {
        bool DoNotDownload;
        int Priority;
        boost::filesystem::path Path;

        FileInfo();
    };

    Box();

    TorrentInfo Torrent;
    std::time_t AddedAt;
    std::time_t CompletedAt;
    bool IsPaused;
    std::uint64_t DownloadedSize;
    std::uint64_t UploadedSize;
    std::uint64_t CorruptedSize;
    boost::filesystem::path SavePath;
    std::uint32_t BlockSize;
    LimitInfo RatioLimit;
    LimitInfo DownloadSpeedLimit;
    LimitInfo UploadSpeedLimit;
    std::vector<FileInfo> Files;
    std::vector<bool> ValidBlocks;
};
