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

#include "BoxHelper.h"

#include "BencodeCodec.h"
#include "Box.h"
#include "Exception.h"
#include "Util.h"

#include <cmath>
#include <sstream>

int BoxHelper::Priority::FromStore(int storeValue, int storeMinValue, int storeMaxValue)
{
    int const boxScaleSize = Box::MaxPriority - Box::MinPriority;
    int const storeScaleSize = storeMaxValue - storeMinValue;
    double const storeMiddleValue = storeMinValue + storeScaleSize / 2.;
    return std::lround(1. * (storeValue - storeMiddleValue) * boxScaleSize / storeScaleSize);
}

int BoxHelper::Priority::ToStore(int boxValue, int storeMinValue, int storeMaxValue)
{
    int const storeScaleSize = storeMaxValue - storeMinValue;
    int const boxScaleSize = Box::MaxPriority - Box::MinPriority;
    double const boxMiddleValue = Box::MinPriority + boxScaleSize / 2.;
    return std::lround(1. * (boxValue - boxMiddleValue) * storeScaleSize / boxScaleSize);
}

void BoxHelper::LoadTorrent(std::istream& stream, Box& box)
{
    BencodeCodec const bencoder;

    bencoder.Decode(stream, box.Torrent);

    if (!box.Torrent.isMember("info"))
    {
        throw Exception("Torrent file is missing info dictionary");
    }

    std::ostringstream infoStream;
    bencoder.Encode(infoStream, box.Torrent["info"]);
    box.InfoHash = Util::CalculateSha1(infoStream.str());
}
