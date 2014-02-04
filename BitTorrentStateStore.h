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

#include "ITorrentStateStore.h"

class BitTorrentStateStore : public ITorrentStateStore
{
public:
    BitTorrentStateStore();
    virtual ~BitTorrentStateStore();

public:
    // ITorrentStateStore
    virtual TorrentClient::Enum GetTorrentClient() const;

    virtual boost::filesystem::path GuessConfigDir() const;
    virtual bool IsValidConfigDir(boost::filesystem::path const& configDir) const;

    virtual ITorrentStateIteratorPtr Export(boost::filesystem::path const& configDir,
        IFileStreamProvider& fileStreamProvider) const;
    virtual void Import(boost::filesystem::path const& configDir, ITorrentStateIteratorPtr boxes,
        IFileStreamProvider& fileStreamProvider) const;
};
