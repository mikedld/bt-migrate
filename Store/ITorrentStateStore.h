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

#include "Common/Exception.h"
#include "Torrent/Intention.h"
#include "Torrent/TorrentClient.h"

#include <memory>

namespace boost::filesystem { class path; }

template<typename... ArgsT>
class IForwardIterator;

struct Box;
typedef IForwardIterator<Box> ITorrentStateIterator;
typedef std::unique_ptr<ITorrentStateIterator> ITorrentStateIteratorPtr;

class IFileStreamProvider;

class ITorrentStateStore
{
public:
    virtual ~ITorrentStateStore();

    virtual TorrentClient::Enum GetTorrentClient() const = 0;

    virtual boost::filesystem::path GuessDataDir(Intention::Enum intention) const = 0;
    virtual bool IsValidDataDir(boost::filesystem::path const& dataDir, Intention::Enum intention) const = 0;

    virtual ITorrentStateIteratorPtr Export(boost::filesystem::path const& dataDir,
        IFileStreamProvider const& fileStreamProvider) const = 0;
    virtual void Import(boost::filesystem::path const& dataDir, Box const& box,
        IFileStreamProvider& fileStreamProvider) const = 0;
};

class ImportCancelledException : public Exception
{
public:
    using Exception::Exception;
    ~ImportCancelledException() override;
};
