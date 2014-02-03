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

#include "Exception.h"
#include "IForwardIterator.h"
#include "ITorrentStateStore.h"
#include "MigrationTransaction.h"
#include "TorrentStateStoreFactory.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <iostream>

namespace fs = boost::filesystem;
namespace po = boost::program_options;

namespace
{

void PrintVersion()
{
    std::cout <<
        "Torrent state migration tool, version 1.0" << std::endl <<
        "Copyright (C) 2014 Mike Gelfand <mikedld@mikedld.com>" << std::endl <<
        std::endl <<
        "This program comes with ABSOLUTELY NO WARRANTY. This is free software," << std::endl <<
        "and you are welcome to redistribute it under certain conditions;" << std::endl <<
        "see <http://www.gnu.org/licenses/gpl.html> for details." << std::endl;
}

void PrintUsage(std::string const& programName, po::options_description const& options)
{
    std::cout <<
        "Usage: " << programName << " [options]" << std::endl <<
        options;
}

ITorrentStateStorePtr FindStateStore(TorrentStateStoreFactory const& storeFactory, std::string const& type,
    std::string& clientName, fs::path& clientConfigDir)
{
    ITorrentStateStorePtr result;

    if (!clientName.empty())
    {
        result = storeFactory.CreateForClient(TorrentClient::FromString(clientName));
        if (clientConfigDir.empty())
        {
            clientConfigDir = result->GuessConfigDir();
            if (clientConfigDir.empty())
            {
                throw Exception("No configuration directory found for " + type + " torrent client");
            }
        }
    }
    else if (!clientConfigDir.empty())
    {
        result = storeFactory.GuessByConfigDir(clientConfigDir);
    }
    else
    {
        throw Exception(type + " torrent client name and/or configuration directory are not specified");
    }

    clientName = TorrentClient::ToString(result->GetTorrentClient());
    clientConfigDir = fs::canonical(clientConfigDir);

    return std::move(result);
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    try
    {
#ifndef _WIN32
        std::locale::global(boost::locale::generator().generate(""));
        fs::path::imbue(std::locale());
#endif

        std::string const programName = fs::path(argv[0]).filename().string();

        std::string sourceName;
        std::string targetName;
        fs::path sourceDir;
        fs::path targetDir;
        bool noBackup = false;
        bool dryRun = false;
        bool verboseOutput = false;

        po::options_description mainOptions("Main options");
        mainOptions.add_options()
            ("source", po::value<std::string>(&sourceName)->value_name("name"), "source client name")
            ("source-dir", po::value<fs::path>(&sourceDir)->value_name("path"), "source client configuration directory")
            ("target", po::value<std::string>(&targetName)->value_name("name"), "target client name")
            ("target-dir", po::value<fs::path>(&targetDir)->value_name("path"), "target client configuration directory")
            ("no-backup", po::bool_switch(&noBackup), "do not backup target client configuration directory")
            ("dry-run", po::bool_switch(&dryRun), "do not write anything to disk");

        po::options_description otherOptions("Other options");
        otherOptions.add_options()
            ("verbose", po::bool_switch(&verboseOutput), "produce verbose output")
            ("version", "print program version")
            ("help", "print this help message");

        po::options_description allOptions;
        allOptions.add(mainOptions);
        allOptions.add(otherOptions);

        po::variables_map args;
        po::store(po::parse_command_line(argc, argv, allOptions), args);
        po::notify(args);

        if (args.count("version") != 0)
        {
            PrintVersion();
            return 0;
        }

        if (args.count("help") != 0)
        {
            PrintVersion();
            std::cout << std::endl;
            PrintUsage(programName, allOptions);
            return 0;
        }

        TorrentStateStoreFactory const storeFactory;

        ITorrentStateStorePtr const sourceStore = FindStateStore(storeFactory, "source", sourceName, sourceDir);
        std::cout << "Source: " << sourceName << " (" << sourceDir << ")" << std::endl;

        ITorrentStateStorePtr const targetStore = FindStateStore(storeFactory, "target", targetName, targetDir);
        std::cout << "Target: " << targetName << " (" << targetDir << ")" << std::endl;

        MigrationTransaction transaction(noBackup, dryRun);

        targetStore->Import(targetDir, sourceStore->Export(sourceDir, transaction), transaction);

        transaction.Commit();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
