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

#include "Common/IFileStreamProvider.h"

#include <boost/filesystem/path.hpp>

#include <mutex>
#include <set>
#include <string>

class MigrationTransaction : public IFileStreamProvider
{
public:
    MigrationTransaction(bool writeThrough, bool dryRun);
    virtual ~MigrationTransaction() noexcept(false);

    void Commit();

public:
    // IFileStreamProvider
    virtual IReadStreamPtr GetReadStream(boost::filesystem::path const& path) const;
    virtual IWriteStreamPtr GetWriteStream(boost::filesystem::path const& path);

private:
    boost::filesystem::path GetTemporaryPath(boost::filesystem::path const& path) const;
    boost::filesystem::path GetBackupPath(boost::filesystem::path const& path) const;

private:
    bool const m_writeThrough;
    bool const m_dryRun;
    std::string const m_transactionId;
    std::set<boost::filesystem::path> m_safePaths;
    std::mutex m_safePathsMutex;
};
