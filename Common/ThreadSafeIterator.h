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

#include "IForwardIterator.h"

#include <mutex>

template<typename... ArgsT>
class ThreadSafeIterator : public IForwardIterator<ArgsT...>
{
public:
    ThreadSafeIterator(std::unique_ptr<IForwardIterator<ArgsT...>> decoratee) :
        m_decoratee(std::move(decoratee)),
        m_mutex()
    {
        //
    }

    ~ThreadSafeIterator() override = default;

public:
    // IForwardIterator
    bool GetNext(ArgsT&... value) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_decoratee->GetNext(std::forward<ArgsT&...>(value...));
    }

private:
    std::unique_ptr<IForwardIterator<ArgsT...>> const m_decoratee;
    std::mutex m_mutex;
};
