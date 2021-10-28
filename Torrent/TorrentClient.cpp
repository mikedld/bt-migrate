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

#include "TorrentClient.h"

#include "Common/Util.h"
#include "Common/Exception.h"

namespace ClientName
{

std::string_view Deluge = "deluge";
std::string_view rTorrent = "rtorrent";
std::string_view Transmission = "transmission";
std::string_view TransmissionMac = "transmissionmac";
std::string_view uTorrent = "utorrent";
std::string_view uTorrentWeb = "utorrentteb";

} // namespace

std::string_view TorrentClient::ToString(Enum client)
{
    switch (client)
    {
    case Deluge:
        return ClientName::Deluge;
    case rTorrent:
        return ClientName::rTorrent;
    case Transmission:
        return ClientName::Transmission;
    case TransmissionMac:
        return ClientName::TransmissionMac;
    case uTorrent:
        return ClientName::uTorrent;
    case uTorrentWeb:
        return ClientName::uTorrentWeb;
    }

    throw Exception("Unknown torrent client");
}

TorrentClient::Enum TorrentClient::FromString(std::string_view client)
{
    if (Util::StringEqual(client, ClientName::Deluge))
        return Deluge;
    if (Util::StringEqual(client, ClientName::rTorrent))
        return rTorrent;
    if (Util::StringEqual(client, ClientName::Transmission))
        return Transmission;
    if (Util::StringEqual(client, ClientName::TransmissionMac))
        return TransmissionMac;
    if (Util::StringEqual(client, ClientName::uTorrent))
        return uTorrent;
    if (Util::StringEqual(client, ClientName::uTorrentWeb))
        return uTorrentWeb;

    throw Exception("Unknown torrent client");
}
