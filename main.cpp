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

#include <csignal>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <thread>

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
                throw Exception(std::format("No data directory found for {} torrent client", lowerCaseClientName));
            }
        }
    }
    else if (!clientDataDir.empty())
    {
        result = storeFactory.GuessByDataDir(clientDataDir, intention);
    }
    else
    {
        throw Exception(std::format("{} torrent client name and/or data directory are not specified", upperCaseClientName));
    }

    clientName = TorrentClient::ToString(result->GetTorrentClient());
    clientDataDir = fs::canonical(clientDataDir);

    if (!result->IsValidDataDir(clientDataDir, intention))
    {
        throw Exception(std::format("Bad {} data directory: {}", lowerCaseClientName, clientDataDir.string()));
    }

    Logger(Logger::Info) << upperCaseClientName << ": " << clientName << " (" << clientDataDir << ")";

    return result;
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        cxxopts::Options options("bt-migrate", "Torrent state migration tool ");

        std::string const programName = fs::path(argv[0]).filename().string();

        // unsigned int maxThreads = std::max(1u, std::thread::hardware_concurrency());

        options.add_options()
    		("source", "source client name", cxxopts::value<std::string>())
            ("source-dir", "source client data directory", cxxopts::value<std::string>())
            ("target", "target client name", cxxopts::value<std::string>())
            ("target-dir", "target client data directory", cxxopts::value<std::string>())
            ("N,max-threads", "maximum number of migration threads", cxxopts::value<unsigned int>()->default_value("0"))
            ("no-backup", "do not backup target client data directory", cxxopts::value<bool>()->default_value("false"))
            ("dry-run", "do not write anything to disk", cxxopts::value<bool>()->default_value("false"))
            ("verbose", "produce verbose output", cxxopts::value<bool>()->default_value("false"))
            ("version", "print program version", cxxopts::value<bool>()->default_value("false"))
            ("help", "print this help message", cxxopts::value<bool>()->default_value("false"))
    	;
        cxxopts::ParseResult args = options.parse(argc, argv);

        if (args.count("help"))
        {
            PrintVersion();
            std::cout << std::endl << options.help() << std::endl;
            return 0;
        }

        if (args.count("version"))
        {
            PrintVersion();
            return 0;
        }

        if (!args.count("source") || !args.count("source-dir") || !args.count("target") || !args.count("target-dir"))
        {
            PrintVersion();
            std::cout << std::endl << options.help() << std::endl;
            return 0;
        }

        std::string sourceName = args["source"].as<std::string>();
        std::string sourceDirString = args["source-dir"].as<std::string>();
        std::string targetName = args["target"].as<std::string>();
        std::string targetDirString = args["target-dir"].as<std::string>();
        unsigned int maxThreads = args["max-threads"].as<unsigned int>();
        bool noBackup = args["no-backup"].as<bool>();
        bool dryRun = args["dry-run"].as<bool>();
        bool verboseOutput = args["verbose"].as<bool>();

        if (verboseOutput)
        {
            Logger::SetMinimumLevel(Logger::Debug);
        }

        TorrentStateStoreFactory const storeFactory;

        fs::path sourceDir = sourceDirString;
        ITorrentStateStorePtr sourceStore = FindStateStore(storeFactory, Intention::Export, sourceName, sourceDir);
        fs::path targetDir = targetDirString;
        ITorrentStateStorePtr targetStore = FindStateStore(storeFactory, Intention::Import, targetName, targetDir);

        unsigned const threadCount = std::max(1u, maxThreads);

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
