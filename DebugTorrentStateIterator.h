#pragma once

#include "IForwardIterator.h"
#include "ITorrentStateStore.h"

class DebugTorrentStateIterator : public ITorrentStateIterator
{
public:
    DebugTorrentStateIterator(ITorrentStateIteratorPtr decoratee);
    virtual ~DebugTorrentStateIterator();

public:
    // ITorrentStateIterator
    virtual bool GetNext(Box& nextBox);

private:
    ITorrentStateIteratorPtr const m_decoratee;
};
