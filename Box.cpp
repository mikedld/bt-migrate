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

#include "Box.h"

Box::LimitInfo::LimitInfo() :
    Mode(LimitMode::Inherit),
    Value(0)
{
    //
}

Box::FileInfo::FileInfo() :
    FullSize(0),
    CurrentSize(0),
    DoNotDownload(false),
    Priority(0),
    LastCheckedAt(0)
{
    //
}

Box::BlockInfo::BlockInfo() :
    IsAvailable(false),
    LastCheckedAt(0)
{
    //
}

Box::Box() :
    InfoHash(),
    Torrent(),
    AddedAt(0),
    CompletedAt(0),
    IsPaused(false),
    DownloadedSize(0),
    UploadedSize(0),
    CorruptedSize(0),
    SavePath(),
    BlockSize(0),
    RatioLimit(),
    DownloadSpeedLimit(),
    UploadSpeedLimit(),
    Files(),
    Blocks()
{
    //
}
