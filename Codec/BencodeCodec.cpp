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
#include "Common/Util.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <iostream>
#include <sstream>

namespace
{

ojson DecodeOneValue(std::istream& stream)
{
    ojson result;

    std::string buffer;
    int c = stream.get();
    switch (c)
    {
    case 'i':
        buffer.clear();
        while ((c = stream.get()) != 'e')
        {
            buffer += static_cast<char>(c);
        }
        result = Util::StringToInt(buffer);
        break;

    case 'l':
        result = ojson::array();
        while (stream.get() != 'e')
        {
            stream.unget();
            result.push_back(DecodeOneValue(stream));
        }
        break;

    case 'd':
        result = ojson::object();
        while (stream.get() != 'e')
        {
            stream.unget();
            ojson key = DecodeOneValue(stream);
            ojson value = DecodeOneValue(stream);
            result.insert_or_assign(key.as<std::string>(), value);
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
            buffer += static_cast<char>(c);
            c = stream.get();
        }
        buffer.resize(Util::StringToInt(buffer));
        stream.read(&buffer[0], buffer.size());
        result = buffer;
        break;

    default:
        throw Exception(fmt::format("Unable to decode value: {}", c));
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
    else if (value.is<std::string>())
    {
        stream << value.as<std::string>().size();
        stream << ':';
        stream << value.as<std::string>();
    }
    else if (value.is<std::intmax_t>())
    {
        stream << 'i';
        stream << value.as<std::intmax_t>();
        stream << 'e';
    }
    else if (value.is<std::uintmax_t>())
    {
        stream << 'i';
        stream << value.as<std::uintmax_t>();
        stream << 'e';
    }
    else
    {
        throw Exception(fmt::format("Unable to encode value: {}", fmt::streamed(value)));
    }
}

} // namespace

BencodeCodec::BencodeCodec() = default;
BencodeCodec::~BencodeCodec() = default;

void BencodeCodec::Decode(std::istream& stream, ojson& root) const
{
    root = DecodeOneValue(stream);
}

void BencodeCodec::Encode(std::ostream& stream, ojson const& root) const
{
    EncodeOneValue(stream, root);
}
