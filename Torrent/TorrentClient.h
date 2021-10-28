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

#include <string>

struct TorrentClient
{
    enum Enum
    {
        Deluge,
        rTorrent,
        Transmission,
        TransmissionMac,
        uTorrent,
        uTorrentWeb
    };

    static Enum const FirstClient = Deluge;
    static Enum const LastClient = uTorrent;

    static std::string_view ToString(Enum client);
    static Enum FromString(std::string_view client);
};
