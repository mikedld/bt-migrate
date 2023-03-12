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

#include "ImportHelper.h"
#include "MigrationTransaction.h"

#include "Common/Exception.h"
#include "Common/Logger.h"
#include "Common/SignalHandler.h"
#include "Store/ITorrentStateStore.h"
#include "Store/TorrentStateStoreFactory.h"
#include "Torrent/Box.h"

#include <cxxopts.hpp>
#include <fmt/format.h>
#include <fmt/std.h>

#include <algorithm>
#include <csignal>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{

void PrintVersion()
{
    std::cout <<
        "Torrent state migration tool, version " << BTMIGRATE_VERSION << std::endl <<
        "Copyright (C) 2014-2021 Mike Gelfand <mikedld@mikedld.com>" << std::endl <<
        std::endl <<
        "This program comes with ABSOLUTELY NO WARRANTY. This is free software," << std::endl <<
        "and you are welcome to redistribute it under certain conditions;" << std::endl <<
        "see <http://www.gnu.org/licenses/gpl.html> for details." << std::endl;
}

void PrintUsage(cxxopts::Options const& options)
{
    std::cout << options.help();
}

ITorrentStateStorePtr FindStateStore(TorrentStateStoreFactory const& storeFactory, Intention::Enum intention,
    std::string& clientName, fs::path& clientDataDir)
{
    std::string const lowerCaseClientName = intention == Intention::Export ? "source" : "target";
    std::string const upperCaseClientName = intention == Intention::Export ? "Source" : "Target";

    ITorrentStateStorePtr result;

    if (!clientName.empty())
    {
        result = storeFactory.CreateForClient(TorrentClient::FromString(clientName));
        if (clientDataDir.empty())
        {
            clientDataDir = result->GuessDataDir(intention);
            if (clientDataDir.empty())
            {
                throw Exception(fmt::format("No data directory found for {} torrent client", lowerCaseClientName));
            }
        }
    }
    else if (!clientDataDir.empty())
    {
        result = storeFactory.GuessByDataDir(clientDataDir, intention);
    }
    else
    {
        throw Exception(fmt::format("{} torrent client name and/or data directory are not specified", upperCaseClientName));
    }

    clientName = TorrentClient::ToString(result->GetTorrentClient());
    clientDataDir = fs::canonical(clientDataDir);

    if (!result->IsValidDataDir(clientDataDir, intention))
    {
        throw Exception(fmt::format("Bad {} data directory: {}", lowerCaseClientName, clientDataDir));
    }

    Logger(Logger::Info) << upperCaseClientName << ": " << clientName << " (" << clientDataDir << ")";

    return result;
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* wideArgv[])
#else
int main(int argc, char* argv[])
#endif
{
    try
    {
#ifdef _WIN32
        auto argvStrings = std::vector<std::string>(argc);
        auto argvCStrings = std::vector<char*>(argc);
        char** const argv = argvCStrings.data();

        for (int i = 0; i < argc; ++i)
        {
            int length = ::WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, nullptr, 0, nullptr, nullptr);
            if (length != 0)
            {
                argvStrings[i].resize(length);
                argvCStrings[i] = argvStrings[i].data();
                length = ::WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, argv[i], length, nullptr, nullptr);
            }

            if (length == 0)
            {
                throw Exception("Failed to parse Win32 command line");
            }
        }
#endif

        std::string const programName = fs::path(argv[0]).filename().string();

        std::string sourceName;
        std::string targetName;
        std::string sourceDirString;
        std::string targetDirString;
        unsigned int maxThreads = std::max(1u, std::thread::hardware_concurrency());
        bool noBackup = false;
        bool dryRun = false;
        bool verboseOutput = false;

        auto options = cxxopts::Options(programName);

        options.add_options("Main")
            ("source", "source client name", cxxopts::value<std::string>(sourceName), "name")
            ("source-dir", "source client data directory", cxxopts::value<std::string>(sourceDirString), "path")
            ("target", "target client name", cxxopts::value<std::string>(targetName), "name")
            ("target-dir", "target client data directory", cxxopts::value<std::string>(targetDirString), "path")
            ("max-threads", "maximum number of migration threads",
                cxxopts::value<unsigned int>(maxThreads)->default_value(std::to_string(maxThreads)), "N")
            ("no-backup", "do not backup target client data directory", cxxopts::value<bool>(noBackup))
            ("dry-run", "do not write anything to disk", cxxopts::value<bool>(dryRun));

        options.add_options("Other")
            ("verbose", "produce verbose output", cxxopts::value<bool>(verboseOutput))
            ("version", "print program version")
            ("help", "print this help message");

        auto const args = options.parse(argc, argv);

        if (args.count("version") != 0)
        {
            PrintVersion();
            return 0;
        }

        if (args.count("help") != 0)
        {
            PrintVersion();
            PrintUsage(options);
            return 0;
        }

        if (verboseOutput)
        {
            Logger::SetMinimumLevel(Logger::Debug);
        }

        TorrentStateStoreFactory const storeFactory;

        fs::path sourceDir = sourceDirString;
        ITorrentStateStorePtr sourceStore = FindStateStore(storeFactory, Intention::Export, sourceName, sourceDir);
        fs::path targetDir = targetDirString;
        ITorrentStateStorePtr targetStore = FindStateStore(storeFactory, Intention::Import, targetName, targetDir);

        unsigned int const threadCount = std::max(1u, maxThreads);

        MigrationTransaction transaction(noBackup, dryRun);

        SignalHandler const signalHandler;

        ImportHelper importHelper(std::move(sourceStore), sourceDir, std::move(targetStore), targetDir, transaction,
            signalHandler);
        ImportHelper::Result const result = importHelper.Import(threadCount);

        bool shouldCommit = true;

        if ((result.FailCount != 0 || result.SkipCount != 0) && !noBackup && !dryRun)
        {
            while (!signalHandler.IsInterrupted())
            {
                std::cout << "Import is not clean, do you want to commit? [yes/no]: " << std::flush;

                std::string answer;
                std::getline(std::cin, answer);

                if (answer == "yes")
                {
                    break;
                }

                if (answer == "no")
                {
                    shouldCommit = false;
                    break;
                }
            }
        }

        if (shouldCommit && !signalHandler.IsInterrupted())
        {
            transaction.Commit();
        }
    }
    catch (std::exception const& e)
    {
        Logger(Logger::Error) << "Error: " << e.what();
        return 1;
    }

    return 0;
}
