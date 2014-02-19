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

#include "MigrationTransaction.h"

#include "Exception.h"
#include "Logger.h"
#include "Throw.h"

#include <boost/date_time.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include <locale>
#include <sstream>

namespace fs = boost::filesystem;
namespace pt = boost::posix_time;

MigrationTransaction::MigrationTransaction(bool writeThrough, bool dryRun) :
    m_writeThrough(writeThrough),
    m_dryRun(dryRun),
    m_transactionId(pt::to_iso_string(pt::microsec_clock::local_time())),
    m_safePaths(),
    m_safePathsMutex()
{
    //
}

MigrationTransaction::~MigrationTransaction()
{
    if (m_writeThrough || m_dryRun)
    {
        return;
    }

    if (m_safePaths.empty())
    {
        return;
    }

    Logger(Logger::Info) << "Reverting changes";

    for (fs::path const& safePath : m_safePaths)
    {
        if (!fs::exists(safePath) && fs::exists(GetBackupPath(safePath)))
        {
            fs::rename(GetBackupPath(safePath), safePath);
        }

        fs::remove(GetTemporaryPath(safePath));
    }
}

void MigrationTransaction::Commit()
{
    if (m_writeThrough || m_dryRun)
    {
        return;
    }

    Logger(Logger::Info) << "Committing changes";

    for (fs::path const& safePath : m_safePaths)
    {
        if (fs::exists(safePath))
        {
            fs::rename(safePath, GetBackupPath(safePath));
        }

        fs::rename(GetTemporaryPath(safePath), safePath);
    }

    m_safePaths.clear();
}

ReadStreamPtr MigrationTransaction::GetReadStream(fs::path const& path)
{
    std::unique_ptr<fs::ifstream> result(new fs::ifstream());
    result->exceptions(std::ios_base::failbit | std::ios_base::badbit);

    try
    {
        result->open(path, std::ios_base::in | std::ios_base::binary);
    }
    catch (std::exception const&)
    {
        Throw<Exception>() << "Unable to open file for reading: " << path;
    }

    return std::move(result);
}

WriteStreamPtr MigrationTransaction::GetWriteStream(fs::path const& path)
{
    static std::string const BlackHoleFilename =
#ifdef _WIN32
        "nul";
#else
        "/dev/null";
#endif

    std::unique_ptr<fs::ofstream> result(new fs::ofstream());
    result->exceptions(std::ios_base::failbit | std::ios_base::badbit);

    try
    {
        if (m_dryRun)
        {
            result->open(BlackHoleFilename, std::ios_base::out | std::ios_base::binary);
        }
        else if (m_writeThrough)
        {
            result->open(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_safePathsMutex);

            result->open(GetTemporaryPath(path), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
            m_safePaths.insert(path);
        }
    }
    catch (std::exception const&)
    {
        Throw<Exception>() << "Unable to open file for writing: " << path;
    }

    return std::move(result);
}

fs::path MigrationTransaction::GetTemporaryPath(fs::path const& path) const
{
    fs::path result = path;
    result += ".tmp." + m_transactionId;
    return result;
}

fs::path MigrationTransaction::GetBackupPath(fs::path const& path) const
{
    fs::path result = path;
    result += ".bak." + m_transactionId;
    return result;
}
