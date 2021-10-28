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

#pragma once

#include <cstddef>
#include <span>
#include <string>


template<typename RangeType, typename RangeValue>
concept range_of = std::ranges::range<RangeType> && std::is_same_v<std::ranges::range_value_t<RangeType>, RangeValue>;

class Sha1
{
	typedef unsigned int(DigestType)[5];

public:
	explicit Sha1()
	{
		Reset();
	}

	explicit Sha1(const std::string& s)
	{
		if (!s.empty()) this->Process(s);
	}

	Sha1& Process(const std::string& s);

	operator std::string();
	std::string GetHash();

	void Reset();
	bool ProcessByte(std::byte byte);
	bool ProcessBytes(std::span<std::byte> bytes);

	template<range_of<std::byte> R>
	bool ProcessBlock(const R& range)
	{
		auto begin = range.begin();
		auto end = range.end();
		while (begin != end)
		{
			if (!ProcessByte(*begin))
				return false;
			++begin;
		}
		return true;
	}

	void GetDigest(DigestType& digest);

private:
	void ProcessBlock();
	void ProcessByteImpl(std::byte byte);

	std::byte m_block[64] = {};
	DigestType m_h;
	size_t m_blockByteIndex;
	size_t m_bitCountLow;
	size_t m_bitCountHigh;
};
