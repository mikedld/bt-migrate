[![Travis Build Status](https://travis-ci.org/mikedld/bt-migrate.svg?branch=master)](https://travis-ci.org/mikedld/bt-migrate)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/7775/badge.svg)](https://scan.coverity.com/projects/mikedld-bt-migrate)

About
==========

Inspired by [old 'wontfix' Transmission ticket](https://trac.transmissionbt.com/ticket/2642) and couple of mentions on [Transmission IRC channel](irc://irc.freenode.net/transmission), here is a tool to ease migration from one BitTorrent client to another.

Building
========

You will need [boost](http://www.boost.org/) library installed.

Clone, generate environment for your favorite build system (we use CMake as abstraction layer), then compile. For example,

    % git clone https://github.com/mikedld/bt-migrate.git
    % cd bt-migrate
    % git submodule update --init --force
    % cmake -G 'Unix Makefiles'
    % make

Running
=======

There are a number of command-line arguments you could use:
  * `--source <NAME>` — BitTorrent client name you're migrating from (see below)
  * `--source-dir <PATH>` — path to source BitTorrent client data directory
  * `--target <NAME>` — BitTorrent client name you're migrating to (see below)
  * `--target-dir <PATH>` — path to target BitTorrent client data directory

Currently supported clients include (names are case-insensitive):
  * "Deluge" (only export)
  * "rTorrent" (only export)
  * "Transmission" (only import)
  * "TransmissionMac" (only import)
  * "uTorrent" (only export)

Whether only `--source`, only `--source-dir`, or both (same goes for target arguments) are required depends on program ability to guess needed information. If client name allows (by checking various places) to find data directory, then the latter is optional. If path to data directory allows (by analyzing its content) to guess corresponding client name, then the latter is optional. Sometimes it's not possible to guess anything, or name/path guessed don't suit you, so both arguments are required.

Some other possible arguments include:
  * `--no-backup` — do not backup (but simply overwrite) any existing files
  * `--dry-run` — do not write anything to disk (useful to check if migration is possible at all)

Example use:

    % ./bt-migrate --source deluge --target-dir ~/.config/transmission --dry-run

For a complete set of arguments, execute:

    % ./bt-migrate --help

License
=======

Code is distributed under the terms of [GNU GPL v3](https://gnu.org/licenses/gpl.html) or later. Feel free to fork, hack and get back to me.
