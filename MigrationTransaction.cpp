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

#include "Common/Exception.h"
#include "Common/Logger.h"
#include "Common/Util.h"

#include <filesystem>
#include <locale>
#include <sstream>
#include <fstream>

namespace fs = std::filesystem;

MigrationTransaction::MigrationTransaction(bool writeThrough, bool dryRun) :
    m_writeThrough(writeThrough),
    m_dryRun(dryRun),
    m_transactionId(Util::GetTimestamp("%Y%m%dT%H%M%S")),
    m_safePaths(),
    m_safePathsMutex()
{
    //
}

MigrationTransaction::~MigrationTransaction() noexcept(false)
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

IReadStreamPtr MigrationTransaction::GetReadStream(fs::path const& path) const
{
    auto result = std::make_unique<std::ifstream>();
    result->exceptions(std::ios_base::failbit | std::ios_base::badbit);

    try
    {
        if (m_safePaths.find(path) != m_safePaths.end())
        {
            result->open(GetTemporaryPath(path), std::ios_base::in | std::ios_base::binary);
        }
        else
        {
            result->open(path, std::ios_base::in | std::ios_base::binary);
        }
    }
    catch (std::exception const&)
    {
        throw Exception(std::format("Unable to open file for reading: {}", path.string()));
    }

    return result;
}

IWriteStreamPtr MigrationTransaction::GetWriteStream(fs::path const& path)
{
    static std::string const BlackHoleFilename =
#ifdef _WIN32
        "nul";
#else
        "/dev/null";
#endif

    auto result = std::make_unique<std::ofstream>();
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
        throw Exception(std::format("Unable to open file for writing: {}", path.string()));
    }

    return result;
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
