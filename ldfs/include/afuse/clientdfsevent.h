// __CR__
// Copyright (c) 2008-2010 Longda Corporation
// All Rights Reserved
// 
// This software contains the intellectual property of Longda Corporation
// or is licensed to Longda Corporation from third parties.  Use of this 
// software and the intellectual property contained therein is expressly
// limited to the terms and conditions of the License Agreement under which 
// it is provided by or on behalf of Longda.
// __CR__


#ifndef __CLIENT_BLOCK_EVENT__
#define __CLIENT_BLOCK_EVENT__

#include "stageevent.h"
#include "clientevent.h"

class CBlockEvent : public StageEvent
{
public:
    CBlockEvent(ClientDfsEvent *pFileEvent, int index):
        mFileEvent(pFileEvent),
        mIndex(index)
    {
    }
public:
    ClientDfsEvent *mFileEvent;
    int             mIndex;
};

class CDownloadBlockEvent : public CBlockEvent
{
public:
    CDownloadBlockEvent(ClientDownloadEvent *pClientDownloadEvent, int index):
        CBlockEvent(pClientDownloadEvent, index)
    {
    }

};

class CUploadBlockEvent : public CBlockEvent
{
public:
    CUploadBlockEvent(ClientUploadEvent *pClientUploadEvent, int index):
        CBlockEvent(pClientUploadEvent, index)
    {
    }

public:
};

class CDeleteBlockEvent : public CBlockEvent
{
public:
    CDeleteBlockEvent(ClientDeleteEvent *pClientDeleteEvent, int index):
        CBlockEvent(pClientDeleteEvent, index)
    {
    }

public:
};

#endif //__CLIENT_BLOCK_EVENT__