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

#include <iostream>
#include <sstream>

namespace
{

ojson DecodeOneValue(std::istream& stream)
{
    ojson result;

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
        result = Util::StringToInt(buffer);
        break;

    case 'l':
        result = ojson::array();
        while (stream.get() != 'e')
        {
            stream.unget();
            result.add(DecodeOneValue(stream));
        }
        break;

    case 'd':
        result = ojson::object();
        while (stream.get() != 'e')
        {
            stream.unget();
            ojson key = DecodeOneValue(stream);
            ojson value = DecodeOneValue(stream);
            result.set(key.as_string(), value);
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

void EncodeOneValue(std::ostream& stream, ojson const& value)
{
    if (value.is_object())
    {
        stream << 'd';
        for (auto const& item : value.object_range())
        {
            EncodeOneValue(stream, item.key());
            EncodeOneValue(stream, item.value());
        }
        stream << 'e';
    }
    else if (value.is_array())
    {
        stream << 'l';
        for (auto const& item : value.array_range())
        {
            EncodeOneValue(stream, item);
        }
        stream << 'e';
    }
    else if (value.is_string())
    {
        stream << value.as_string().size();
        stream << ':';
        stream << value.as_string();
    }
    else if (value.is_integer())
    {
        stream << 'i';
        stream << value.as_integer();
        stream << 'e';
    }
    else if (value.is_uinteger())
    {
        stream << 'i';
        stream << value.as_uinteger();
        stream << 'e';
    }
    else
    {
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

void BencodeCodec::Decode(std::istream& stream, ojson& root) const
{
    root = DecodeOneValue(stream);
}

void BencodeCodec::Encode(std::ostream& stream, ojson const& root) const
{
    EncodeOneValue(stream, root);
}
