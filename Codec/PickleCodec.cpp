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
#include "Common/Throw.h"
#include "Common/Util.h"

#include <json/value.h>
#include <json/writer.h>

#include <iostream>
#include <map>
#include <stack>

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
    PROTO       = '\x80',
    NEWOBJ      = '\x81',
    EXT1        = '\x82',
    EXT2        = '\x83',
    EXT4        = '\x84',
    TUPLE1      = '\x85',
    TUPLE2      = '\x86',
    TUPLE3      = '\x87',
    NEWTRUE     = '\x88',
    NEWFALSE    = '\x89',
    LONG1       = '\x8a',
    LONG4       = '\x8b',

    /* Protocol 3 (Python 3.x) */
    BINBYTES       = 'B',
    SHORT_BINBYTES = 'C'
};

struct StackItem
{
    StackItem() :
        Type(0),
        Value()
    {

    }

    explicit StackItem(char type, Json::Value const& value = Json::Value()) :
        Type(type),
        Value(value)
    {
        //
    }

    char Type;
    Json::Value Value;
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

        unsigned int code = std::strtol(codeBuffer.c_str(), NULL, 16);
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

            unsigned int const code2 = std::strtol(codeBuffer.c_str(), NULL, 16);
            if (code2 <= 0xdc00 && code2 >= 0xdfff)
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

} // namespace

PickleCodec::PickleCodec()
{
    //
}

PickleCodec::~PickleCodec()
{
    //
}

void PickleCodec::Decode(std::istream& stream, Json::Value& root) const
{
    std::stack<StackItem> stack;
    std::map<long long, StackItem> memo;

    bool stopped = false;
    StackItem currentItem, currentItem2;
    std::string buffer;
    while (!stopped && !stream.eof())
    {
        int const code = stream.get();
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
                stack.push(StackItem(code, static_cast<Json::Value::Int64>(Util::StringToInt(buffer))));
            }
            break;

        case FLOAT:
            std::getline(stream, buffer, '\n');
            stack.push(StackItem(code, std::strtod(buffer.c_str(), NULL)));
            break;

        case LONG:
            std::getline(stream, buffer, '\n');
            if (!buffer.empty() && buffer.back() == 'L')
            {
                buffer.resize(buffer.size() - 1);
            }
            stack.push(StackItem(code, static_cast<Json::Value::Int64>(Util::StringToInt(buffer))));
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
            // fall through

        case DICT:
            currentItem = StackItem(code, Json::objectValue);
            while (stack.top().Type != MARK)
            {
                currentItem2 = stack.top();
                stack.pop();
                currentItem.Value[stack.top().Value.asString()] = currentItem2.Value;
                stack.pop();
            }
            stack.pop();
            stack.push(currentItem);
            break;

        case LIST:
        case TUPLE:
            currentItem = StackItem(code, Json::arrayValue);
            while (stack.top().Type != MARK)
            {
                currentItem.Value.append(stack.top().Value);
                stack.pop();
            }
            stack.pop();
            stack.push(currentItem);
            break;

        case NONE:
            stack.push(StackItem(code, Json::nullValue));
            break;

        case EMPTY_DICT:
            stack.push(StackItem(code, Json::objectValue));
            break;

        case EMPTY_LIST:
        case EMPTY_TUPLE:
            stack.push(StackItem(code, Json::arrayValue));
            break;

        case APPEND:
            currentItem = stack.top();
            stack.pop();
            stack.top().Value.append(currentItem.Value);
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
            stack.top().Value[currentItem2.Value.asString()] = currentItem.Value;
            break;

        // case PERSID:
        // case REDUCE:
        // case GLOBAL:
        // case APPENDS:
        // case OBJ:
        // case SETITEMS:
        // case BINGET:
        // case LONG_BINGET:
        // case BINPUT:
        // case LONG_BINPUT:
        // case BINPERSID:
        // case BINFLOAT:
        // case BININT:
        // case BININT1:
        // case BININT2:
        // case BINSTRING:
        // case SHORT_BINSTRING:
        // case BINUNICODE:
        default:
            Throw<Exception>() << "Pickle opcode " << code << " not yet supported";
        }
    }

    if (!stack.empty())
    {
        throw Exception("Pickle stack is not empty at the end");
    }
}

void PickleCodec::Encode(std::ostream& /*stream*/, Json::Value const& /*root*/) const
{
    throw NotImplementedException(__func__);
}
