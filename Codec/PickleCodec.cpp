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

#include "PickleCodec.h"

#include "Common/Exception.h"
#include "Common/Util.h"

#include <fmt/format.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iostream>
#include <map>
#include <stack>
#include <type_traits>

namespace
{

// Taken from Python sources
enum opcode
{
    MARK            = '(',
    STOP            = '.',
    POP             = '0',
    POP_MARK        = '1',
    DUP             = '2',
    FLOAT           = 'F',
    INT             = 'I',
    BININT          = 'J',
    BININT1         = 'K',
    LONG            = 'L',
    BININT2         = 'M',
    NONE            = 'N',
    PERSID          = 'P',
    BINPERSID       = 'Q',
    REDUCE          = 'R',
    STRING          = 'S',
    BINSTRING       = 'T',
    SHORT_BINSTRING = 'U',
    UNICODE_        = 'V',
    BINUNICODE      = 'X',
    APPEND          = 'a',
    BUILD           = 'b',
    GLOBAL          = 'c',
    DICT            = 'd',
    EMPTY_DICT      = '}',
    APPENDS         = 'e',
    GET             = 'g',
    BINGET          = 'h',
    INST            = 'i',
    LONG_BINGET     = 'j',
    LIST            = 'l',
    EMPTY_LIST      = ']',
    OBJ             = 'o',
    PUT             = 'p',
    BINPUT          = 'q',
    LONG_BINPUT     = 'r',
    SETITEM         = 's',
    TUPLE           = 't',
    EMPTY_TUPLE     = ')',
    SETITEMS        = 'u',
    BINFLOAT        = 'G',

    /* Protocol 2. */
    PROTO       = 0x80,
    NEWOBJ      = 0x81,
    EXT1        = 0x82,
    EXT2        = 0x83,
    EXT4        = 0x84,
    TUPLE1      = 0x85,
    TUPLE2      = 0x86,
    TUPLE3      = 0x87,
    NEWTRUE     = 0x88,
    NEWFALSE    = 0x89,
    LONG1       = 0x8a,
    LONG4       = 0x8b,

    /* Protocol 3 (Python 3.x) */
    BINBYTES       = 'B',
    SHORT_BINBYTES = 'C',

    /* Protocol 4 */
    SHORT_BINUNICODE = 0x8c,
    BINUNICODE8      = 0x8d,
    BINBYTES8        = 0x8e,
    EMPTY_SET        = 0x8f,
    ADDITEMS         = 0x90,
    FROZENSET        = 0x91,
    NEWOBJ_EX        = 0x92,
    STACK_GLOBAL     = 0x93,
    MEMOIZE          = 0x94,
    FRAME            = 0x95,

    /* Protocol 5 */
    BYTEARRAY8       = 0x96,
    NEXT_BUFFER      = 0x97,
    READONLY_BUFFER  = 0x98
};

struct StackItem
{
    StackItem() :
        Type(0),
        Value()
    {

    }

    explicit StackItem(int type, ojson const& value = ojson()) :
        Type(type),
        Value(value)
    {
        //
    }

    int Type;
    ojson Value;
};

std::string UnicodeCodePointToUtf8(unsigned int code)
{
    std::string result;

    if (code <= 0x7f)
    {
        result.resize(1);
        result[0] = static_cast<char>(code);
    }
    else if (code <= 0x7FF)
    {
        result.resize(2);
        result[1] = static_cast<char>(0x80 | (0x3f & code));
        result[0] = static_cast<char>(0xc0 | (0x1f & (code >> 6)));
    }
    else if (code <= 0xFFFF)
    {
        result.resize(3);
        result[2] = static_cast<char>(0x80 | (0x3f & code));
        result[1] = static_cast<char>(0x80 | (0x3f & (code >> 6)));
        result[0] = static_cast<char>(0xe0 | (0x0f & (code >> 12)));
    }
    else if (code <= 0x10FFFF)
    {
        result.resize(4);
        result[3] = static_cast<char>(0x80 | (0x3f & code));
        result[2] = static_cast<char>(0x80 | (0x3f & (code >> 6)));
        result[1] = static_cast<char>(0x80 | (0x3f & (code >> 12)));
        result[0] = static_cast<char>(0xf0 | (0x07 & (code >> 18)));
    }
    else
    {
        throw Exception("Invalid unicode code point");
    }

    return result;
}

std::string UnicodeToUtf8(std::string const& text)
{
    std::string result;

    for (auto it = text.cbegin(), end = text.cend(); it != end; ++it)
    {
        if (*it != '\\')
        {
            result += *it;
            continue;
        }

        ++it;
        switch (*it)
        {
        case 'u':
            break;
        case 'b':
            result += '\b';
            continue;
        case 'f':
            result += '\f';
            continue;
        case 'n':
            result += '\n';
            continue;
        case 'r':
            result += '\r';
            continue;
        case 't':
            result += '\t';
            continue;
        case '"':
        case '\\':
        default:
            result += *it;
            continue;
        }

        std::string codeBuffer;
        codeBuffer += *(++it);
        codeBuffer += *(++it);
        codeBuffer += *(++it);
        codeBuffer += *(++it);

        unsigned int code = std::strtol(codeBuffer.c_str(), nullptr, 16);
        if (code >= 0xd800 && code <= 0xdbff)
        {
            if (*(++it) != '\\')
            {
                throw Exception("Invalid unicode code point");
            }

            if (*(++it) != 'u')
            {
                throw Exception("Invalid unicode code point");
            }

            codeBuffer.clear();
            codeBuffer += *(++it);
            codeBuffer += *(++it);
            codeBuffer += *(++it);
            codeBuffer += *(++it);

            unsigned int const code2 = std::strtol(codeBuffer.c_str(), nullptr, 16);
            if (code2 >= 0xdc00 && code2 <= 0xdfff)
            {
                throw Exception("Invalid unicode code point");
            }

            code = ((code - 0xd800) << 10) | (code2 - 0xdc00);
        }

        result += UnicodeCodePointToUtf8(code);
    }

    return result;
}

void PopMark(std::stack<StackItem>& stack)
{
    while (!stack.empty() && stack.top().Type != MARK)
    {
        stack.pop();
    }
    if (!stack.empty())
    {
        stack.pop();
    }
}

template<typename T>
T GetBinNumber(std::istream& stream, std::endian order = std::endian::little)
{
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);

    char result[sizeof(T)];
    stream.read(result, sizeof(result));

    if (order != std::endian::native)
    {
        std::reverse(result, result + sizeof(result));
    }

    return *reinterpret_cast<const T*>(result);
}

template<typename LengthT>
std::string GetBinUnicode(std::istream& stream, std::endian order = std::endian::little)
{
    LengthT const length = GetBinNumber<LengthT>(stream, order);

    std::string result;
    result.resize(length);
    stream.read(result.data(), length);
    return result;
}

} // namespace

PickleCodec::PickleCodec() = default;
PickleCodec::~PickleCodec() = default;

void PickleCodec::Decode(std::istream& stream, ojson& root) const
{
    std::stack<StackItem> stack, stack2;
    std::map<long long, StackItem> memo;

    [[maybe_unused]] int proto = 0;

    bool stopped = false;
    StackItem currentItem, currentItem2;
    std::string buffer, buffer2;
    while (!stopped && !stream.eof())
    {
        int const code = GetBinNumber<std::uint8_t>(stream);
        switch (code)
        {
        case MARK:
            stack.push(StackItem(code));
            break;

        case STOP:
            root = stack.top().Value;
            stack.pop(); // result
            stopped = true;
            break;

        case POP:
            stack.pop();
            break;

        case POP_MARK:
            PopMark(stack);
            break;

        case DUP:
            stack.push(stack.top());
            break;

        case INT:
            std::getline(stream, buffer, '\n');
            if (buffer == "00")
            {
                stack.push(StackItem(code, false));
            }
            else if (buffer == "01")
            {
                stack.push(StackItem(code, true));
            }
            else
            {
                stack.push(StackItem(code, Util::StringToInt(buffer)));
            }
            break;

        case FLOAT:
            std::getline(stream, buffer, '\n');
            stack.push(StackItem(code, std::strtod(buffer.c_str(), nullptr)));
            break;

        case LONG:
            std::getline(stream, buffer, '\n');
            if (!buffer.empty() && buffer.back() == 'L')
            {
                buffer.resize(buffer.size() - 1);
            }
            stack.push(StackItem(code, Util::StringToInt(buffer)));
            break;

        case STRING:
            std::getline(stream, buffer, '\n');
            stack.push(StackItem(code, UnicodeToUtf8(buffer.substr(1, buffer.size() - 2))));
            break;

        case UNICODE_:
            std::getline(stream, buffer, '\n');
            stack.push(StackItem(code, UnicodeToUtf8(buffer)));
            break;

        case INST:
            std::getline(stream, buffer, '\n');
            std::getline(stream, buffer, '\n');
            [[fallthrough]];

        case DICT:
            currentItem = StackItem(code, ojson::object());
            while (stack.top().Type != MARK)
            {
                currentItem2 = stack.top();
                stack.pop();
                currentItem.Value.insert_or_assign(stack.top().Value.as<std::string>(), currentItem2.Value);
                stack.pop();
            }
            stack.top() = currentItem;
            break;

        case LIST:
        case TUPLE:
            currentItem = StackItem(code, ojson::array());
            while (stack.top().Type != MARK)
            {
                currentItem.Value.push_back(stack.top().Value);
                stack.pop();
            }
            stack.top() = currentItem;
            break;

        case NONE:
            stack.push(StackItem(code, ojson::null()));
            break;

        case EMPTY_DICT:
            stack.push(StackItem(code, ojson::object()));
            break;

        case EMPTY_LIST:
            stack.push(StackItem(code, ojson::array()));
            break;

        case TUPLE3:
            stack2.push(std::move(stack.top()));
            stack.pop();
            [[fallthrough]];

        case TUPLE2:
            stack2.push(std::move(stack.top()));
            stack.pop();
            [[fallthrough]];

        case TUPLE1:
            stack2.push(std::move(stack.top()));
            stack.pop();
            [[fallthrough]];

        case EMPTY_TUPLE:
            stack.push(StackItem(code, ojson::array()));
            while (!stack2.empty())
            {
                stack.top().Value.push_back(std::move(stack2.top().Value));
                stack2.pop();
            }
            break;

        case APPEND:
            currentItem = stack.top();
            stack.pop();
            stack.top().Value.push_back(currentItem.Value);
            break;

        case BUILD:
            currentItem = stack.top();
            stack.pop();
            stack.top() = currentItem;
            break;

        case GET:
            std::getline(stream, buffer, '\n');
            stack.push(memo.at(Util::StringToInt(buffer)));
            break;

        case PUT:
            std::getline(stream, buffer, '\n');
            memo[Util::StringToInt(buffer)] = stack.top();
            break;

        case SETITEM:
            currentItem = stack.top();
            stack.pop();
            currentItem2 = stack.top();
            stack.pop();
            stack.top().Value.insert_or_assign(currentItem2.Value.as<std::string>(), currentItem.Value);
            break;

        case PROTO:
            proto = static_cast<unsigned int>(stream.get());
            break;

        case GLOBAL:
            std::getline(stream, buffer, '\n');
            std::getline(stream, buffer2, '\n');
            stack.push(StackItem(code, buffer + ':' + buffer2));
            break;

        case BINPUT:
            memo[GetBinNumber<std::uint8_t>(stream)] = stack.top();
            break;

        case NEWOBJ:
            currentItem = stack.top();
            stack.pop();
            currentItem2 = stack.top();
            stack.pop();
            stack.push(StackItem(code, ojson::object()));
            break;

        case BINUNICODE:
            stack.push(StackItem(code, GetBinUnicode<std::uint32_t>(stream)));
            break;

        case NEWTRUE:
            stack.push(StackItem(code, true));
            break;

        case NEWFALSE:
            stack.push(StackItem(code, false));
            break;

        case BINFLOAT:
            stack.push(StackItem(code, GetBinNumber<double>(stream, std::endian::big)));
            break;

        case BININT1:
            stack.push(StackItem(code, GetBinNumber<std::int8_t>(stream)));
            break;

        case APPENDS:
            while (!stack.empty() && stack.top().Type != MARK)
            {
                stack2.push(std::move(stack.top()));
                stack.pop();
            }
            stack.pop();
            while (!stack2.empty())
            {
                stack.top().Value.push_back(std::move(stack2.top().Value));
                stack2.pop();
            }
            break;

        case BININT:
            stack.push(StackItem(code, GetBinNumber<std::int32_t>(stream)));
            break;

        case BINGET:
            stack.push(memo[GetBinNumber<std::uint8_t>(stream)]);
            break;

        case SETITEMS:
            while (!stack.empty() && stack.top().Type != MARK)
            {
                stack2.push(std::move(stack.top()));
                stack.pop();
            }
            stack.pop();
            while (!stack2.empty())
            {
                currentItem = std::move(stack2.top());
                stack2.pop();
                currentItem2 = std::move(stack2.top());
                stack2.pop();
                stack.top().Value.insert_or_assign(currentItem.Value.as<std::string>(), std::move(currentItem2.Value));
            }
            break;

        case LONG_BINPUT:
            memo[GetBinNumber<std::uint32_t>(stream)] = stack.top();
            break;

        case LONG_BINGET:
            stack.push(memo[GetBinNumber<std::uint32_t>(stream)]);
            break;

        default:
            throw Exception(fmt::format("Pickle opcode {} not yet supported", code));
        }
    }

    if (!stack.empty())
    {
        throw Exception("Pickle stack is not empty at the end");
    }
}

void PickleCodec::Encode(std::ostream& /*stream*/, ojson const& /*root*/) const
{
    throw NotImplementedException(__func__);
}
