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

#include "Common/Exception.h"

#include <boost/algorithm/string/predicate.hpp>

namespace ClientName
{

std::string const Deluge = "Deluge";
std::string const rTorrent = "rTorrent";
std::string const Transmission = "Transmission";
std::string const uTorrent = "uTorrent";

} // namespace

std::string TorrentClient::ToString(Enum client)
{
    switch (client)
    {
    case Deluge:
        return ClientName::Deluge;
    case rTorrent:
        return ClientName::rTorrent;
    case Transmission:
        return ClientName::Transmission;
    case uTorrent:
        return ClientName::uTorrent;
    }

    throw Exception("Unknown torrent client");
}

TorrentClient::Enum TorrentClient::FromString(std::string client)
{
    using boost::algorithm::iequals;

    if (iequals(client, ClientName::Deluge))
    {
        return Deluge;
    }
    else if (iequals(client, ClientName::rTorrent))
    {
        return rTorrent;
    }
    else if (iequals(client, ClientName::Transmission))
    {
        return Transmission;
    }
    else if (iequals(client, ClientName::uTorrent))
    {
        return uTorrent;
    }

    throw Exception("Unknown torrent client");
}
