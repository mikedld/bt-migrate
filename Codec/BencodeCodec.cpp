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

#include "BencodeCodec.h"

#include "Common/Exception.h"
#include "Common/Throw.h"
#include "Common/Util.h"

#include <json/value.h>
#include <json/writer.h>

#include <iostream>
#include <sstream>

namespace
{

Json::Value DecodeOneValue(std::istream& stream)
{
    Json::Value result;

    std::string buffer;
    char c = stream.get();
    switch (c)
    {
    case 'i':
        buffer.clear();
        while ((c = stream.get()) != 'e')
        {
            buffer += c;
        }
        result = static_cast<Json::Value::Int64>(Util::StringToInt(buffer));
        break;

    case 'l':
        result = Json::arrayValue;
        while (stream.get() != 'e')
        {
            stream.unget();
            result.append(DecodeOneValue(stream));
        }
        break;

    case 'd':
        result = Json::objectValue;
        while (stream.get() != 'e')
        {
            stream.unget();
            Json::Value key = DecodeOneValue(stream);
            Json::Value value = DecodeOneValue(stream);
            result[key.asString()] = value;
        }
        break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        buffer.clear();
        while (c != ':')
        {
            buffer += c;
            c = stream.get();
        }
        buffer.resize(Util::StringToInt(buffer));
        stream.read(&buffer[0], buffer.size());
        result = buffer;
        break;

    default:
        Throw<Exception>() << "Unable to decode value: " << static_cast<int>(c);
    }

    return result;
}

void EncodeOneValue(std::ostream& stream, Json::Value const& value)
{
    switch (value.type())
    {
    case Json::objectValue:
        stream << 'd';
        for (auto it = value.begin(), end = value.end(); it != end; ++it)
        {
            EncodeOneValue(stream, it.key());
            EncodeOneValue(stream, *it);
        }
        stream << 'e';
        break;

    case Json::arrayValue:
        stream << 'l';
        for (auto it = value.begin(), end = value.end(); it != end; ++it)
        {
            EncodeOneValue(stream, *it);
        }
        stream << 'e';
        break;

    case Json::stringValue:
        stream << value.asString().size();
        stream << ':';
        stream << value.asString();
        break;

    case Json::intValue:
        stream << 'i';
        stream << value.asInt64();
        stream << 'e';
        break;

    case Json::uintValue:
        stream << 'i';
        stream << value.asUInt64();
        stream << 'e';
        break;

    default:
        Throw<Exception>() << "Unable to encode value: " << value;
    }
}

} // namespace

BencodeCodec::BencodeCodec()
{
    //
}

BencodeCodec::~BencodeCodec()
{
    //
}

void BencodeCodec::Decode(std::istream& stream, Json::Value& root) const
{
    root = DecodeOneValue(stream);
}

void BencodeCodec::Encode(std::ostream& stream, Json::Value const& root) const
{
    EncodeOneValue(stream, root);
}
