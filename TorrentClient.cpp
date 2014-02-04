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

#include "Exception.h"

#include <boost/algorithm/string.hpp>

namespace ClientName
{

std::string const BitTorrent = "BitTorrent";
std::string const Deluge = "Deluge";
std::string const Transmission = "Transmission";

} // namespace

std::string TorrentClient::ToString(Enum client)
{
    switch (client)
    {
    case BitTorrent:
        return ClientName::BitTorrent;
    case Deluge:
        return ClientName::Deluge;
    case Transmission:
        return ClientName::Transmission;
    }

    throw Exception("Unknown torrent client");
}

TorrentClient::Enum TorrentClient::FromString(std::string client)
{
    using boost::algorithm::iequals;

    if (iequals(client, ClientName::BitTorrent))
    {
        return BitTorrent;
    }
    else if (iequals(client, ClientName::Deluge))
    {
        return Deluge;
    }
    else if (iequals(client, ClientName::Transmission))
    {
        return Transmission;
    }

    throw Exception("Unknown torrent client");
}
