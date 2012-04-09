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


// Include Files

// Include Files
#include "log.h"

#include "clientdfstage.h"
#include "timerstage.h"

#include "clientevent.h"
#include "metadataevent.h"
#include "clientdfsevent.h"


#include "fdfs.h"
#include "metacache.h"

//! Constructor
ClientDfsStage::ClientDfsStage(const char* tag) :
    Stage(tag),
    mBlockSize(ADFS_DEFAULT_BLOCK_SIZE)
{
}


//! Destructor
ClientDfsStage::~ClientDfsStage()
{
}

//! Parse properties, instantiate a stage object
Stage*
ClientDfsStage::makeStage(const std::string& tag)
{
    ClientDfsStage* retValue = new ClientDfsStage(tag.c_str());
    if( retValue == NULL )
    {
        LOG_ERROR("new ClientDfsStage failed");
        return NULL;
    }
    retValue->setProperties();
    return retValue;
}

//! Set properties for this object set in stage specific properties
bool
ClientDfsStage::setProperties()
{
    std::string key;
    std::string value;

    key = "BLOCKSIZE";
    value = theGlobalProperties()->GetParamValue(key, getName());
    if (value.size())
    {
        Xlate::strToVal(value, mBlockSize);
    }
    
    return true;
}

//! Initialize stage params and validate outputs
bool
ClientDfsStage::initialize()
{
    LOG_TRACE("Enter");
    
    std::list<Stage*>::iterator stgp = nextStageList.begin();
    mTimerStage = *(stgp++);

    TimerStage *timerStage = NULL;
    timerStage = dynamic_cast<TimerStage *>(mTimerStage);
    ASSERT( timerStage, "The next stage isn't TimerStage" );

    LOG_TRACE("Exit");
    return true;
}

//! Cleanup after disconnection
void
ClientDfsStage::cleanup()
{
    LOG_TRACE("Enter");
    
   
    LOG_TRACE("Exit");
}

void
ClientDfsStage::handleEvent(StageEvent* event)
{
    LOG_TRACE("Enter\n");

    if (dynamic_cast<ClientTestEvent *> (event) )
    {
        handleClientTest(event);
        return ;
    }
    else if (dynamic_cast<ClientDownloadEvent *>(event))
    {
        DownloadFile(event);
        return ;
    }
    else if (dynamic_cast<CDownloadBlockEvent *>(event))
    {
        DownloadBlock(event);
        return ;
    }
    else if (dynamic_cast<ClientUploadEvent *>(event))
    {
        UploadFile(event);
        return ;
    }
    else if (dynamic_cast<CUploadBlockEvent *>(event))
    {
        UploadBlock(event);
        return ;
    }
    else if (dynamic_cast<ClientDeleteEvent *>(event))
    {
        DeleteFile(event);
        return ;
    }
    else if (dynamic_cast<CDeleteBlockEvent *>(event))
    {
        DeleteBlock(event);
        return ;
    }
    
    LOG_TRACE("Exit\n");
    return ;
}


void 
ClientDfsStage::callbackEvent(StageEvent* event, CallbackContext* context)
{
    LOG_TRACE("Enter\n");

    if (dynamic_cast<ClientTestEvent *> (event) )
    {
        handleClientTestCb(event);
        return ;
    }
    
    LOG_TRACE("Exit\n");
    return ;
}

void
ClientDfsStage::handleClientTest(StageEvent *event)
{
    ClientTestEvent *clientTestEvent = dynamic_cast<ClientTestEvent *>(event);

    LOG_INFO("%s Handle ClientTestEvent", __FUNCTION__);

    CompletionCallback *cb = new CompletionCallback(this, NULL);
    if (cb == NULL)
    {
        LOG_ERROR("Failed to new callback");
        
        clientTestEvent->mError = ENOMEM;

        clientTestEvent->done();

        return ;
    }

    clientTestEvent->pushCallback(cb);

    TimerRegisterEvent *tmEvent = new TimerRegisterEvent(clientTestEvent, 15 * USEC_PER_SEC);

    mTimerStage->addEvent(tmEvent);

    return ;
}

void
ClientDfsStage::handleClientTestCb(StageEvent *event)
{
    ClientTestEvent *clientTestEvent = dynamic_cast<ClientTestEvent *>(event);

    LOG_INFO("%s Handle ClientTestEvent", __FUNCTION__);

    clientTestEvent->mError = 0;

    clientTestEvent->done();
    LOG_INFO("Finish handle");
}

void
ClientDfsStage::DownloadFile(StageEvent *event)
{
    ClientDownloadEvent *pDownloadEvent = dynamic_cast<ClientDownloadEvent *>(event);

    if (pDownloadEvent->mBlockValue.empty() == true)
    {
        pDownloadEvent->done();
        
        return ;
    }

    int ret = STATUS_SUCCESS;

    if (pDownloadEvent->mType == ADFS_DATA_GEOG_SIMPLE)
    {
        s64_t  fileSize = 0;
            
        ret = GlobalDfsInstance().DownloadFile(pDownloadEvent->mFileName.c_str(), 
            pDownloadEvent->mBlockValue.c_str(), &fileSize);
        if (ret)
        {
            LOG_ERROR("Failed to download %s to %s, rc:%d:%s",
                pDownloadEvent->mBlockValue.c_str(), pDownloadEvent->mFileName.c_str(),
                ret, strerror(ret));

            pDownloadEvent->mError = ret;
            pDownloadEvent->mErrMsg = "Failed to download " + pDownloadEvent->mBlockValue;

            pDownloadEvent->done();

            return ;
        }

        pDownloadEvent->done();
        
        return ;
    }

    DataGeography      *pDGMeta = NULL;
    int                 dgLen = 0;

    ret = GlobalDGCache()->GetKeyV(pDownloadEvent->mBlockValue.c_str(), (char **)&pDGMeta, &dgLen, META_READ);
    if (ret)
    {
        LOG_ERROR("Failed to get DataGeography of %s, rc:%d:%s",
                pDownloadEvent->mBlockValue.c_str(), ret, strerror(ret));

        pDownloadEvent->mError = ret;
        pDownloadEvent->mErrMsg = "Failed to get DataGeography of " + pDownloadEvent->mBlockValue;

        pDownloadEvent->done();

        return ;
    }

    DataGeography *pDG = (DataGeography *)new char[dgLen];
    memcpy(pDG, pDGMeta, dgLen);

    GlobalDGCache()->PutKeyV(pDownloadEvent->mBlockValue.c_str(), false, META_READ);

    pDownloadEvent->mDG.reset(pDG);
    

    int sendEventNum = 0;
    for(int i = 0; i < pDG->mBlockNum; i++)
    {
        CDownloadBlockEvent *pDownloadBlockEv = new CDownloadBlockEvent(pDownloadEvent, i);
        if (pDownloadBlockEv == NULL)
        {
            LOG_ERROR("Failed to alloc memory for CDownloadBlockEvent %s:%d",
                pDownloadEvent->mFileName.c_str(), i);

            pDownloadEvent->mError = ENOMEM;
            pDownloadEvent->mErrMsg = "Failed to alloc memory for CDownloadBlockEvent "  + 
                pDownloadEvent->mFileName;
            break;
            
        }

        pDownloadEvent->mBlockEvents.insert((StageEvent *)pDownloadBlockEv);

        this->addEvent(pDownloadBlockEv);

        sendEventNum++;
    }

    if (sendEventNum == 0)
    {
        pDownloadEvent->done();
    }

    return ;
}

void 
ClientDfsStage::DownloadBlock(StageEvent *event)
{
    CDownloadBlockEvent *pDownloadBlockEvent = dynamic_cast<CDownloadBlockEvent * >(event);

    ClientDownloadEvent *pDownloadFileEvent  = (ClientDownloadEvent*)pDownloadBlockEvent->mFileEvent;

    DataGeography       &dataGeography       = *pDownloadFileEvent->mDG.get();

    BlockGeography      &blockGeography      = dataGeography.mBlocks[pDownloadBlockEvent->mIndex];

    int ret = 0;
    
    if (dataGeography.mBlockNum == 1)
    {
        ret = GlobalDfsInstance().DownloadFile(pDownloadFileEvent->mFileName.c_str(),
            blockGeography.mBlockId, (s64_t *)&blockGeography.mLen);
    }
    else
    {
        ret = GlobalDfsInstance().DownloadFilePart(pDownloadFileEvent->mFileName.c_str(),
            (s64_t)blockGeography.mOffset, blockGeography.mBlockId, (s64_t *)&blockGeography.mLen);
    }

    if (ret)
    {
        LOG_ERROR("Failed to download to part file %s, offset:%llu, rc:%d:%s",
            pDownloadFileEvent->mFileName.c_str(), blockGeography.mOffset,
            ret, strerror(ret));

        pDownloadFileEvent->mError = ret;
        pDownloadFileEvent->mErrMsg= "Failed to download to part file " + 
            pDownloadFileEvent->mFileName + 
            " failed in " + blockGeography.mBlockId;
        // the left operation is same as good operation
        
    }

    pDownloadFileEvent->CheckFinished((StageEvent *)pDownloadBlockEvent);

    pDownloadBlockEvent->done();

    return ;
}

void
ClientDfsStage::UploadFile(StageEvent *event)
{
    int ret = STATUS_SUCCESS;
    
    ClientUploadEvent *pUploadFileEvent = dynamic_cast<ClientUploadEvent *>(event);

    const char *pLocalFile = pUploadFileEvent->mFileName.c_str();

    struct stat tempStat;
    memset(&tempStat, 0, sizeof(tempStat));
    ret = stat(pLocalFile, &tempStat);
    if (ret)
    {
        LOG_ERROR("Failed to stat %s, rc:%d:%s", pLocalFile, errno, strerror(errno));

        pUploadFileEvent->done();

        return ;
    }

    int blockNum  = 0;
    int allocSize = 0;
    
    if (tempStat.st_size == 0 )
    {
        blockNum = 0;
    }
    else
    {
        blockNum = (tempStat.st_size + mBlockSize - 1)/mBlockSize;
    }

    allocSize = sizeof(DataGeography) + blockNum * sizeof(BlockGeography);
    DataGeography *pDG = (DataGeography *)new char[allocSize];
    if (pDG == NULL)
    {
        LOG_ERROR("Failed to alloc DataGeography, part file :%s", pLocalFile);

        pUploadFileEvent->mError  = ENOMEM;
        pUploadFileEvent->mErrMsg = "Failed to alloc DataGeography, part file :" + pUploadFileEvent->mFileName;
        pUploadFileEvent->done();

        return ;
    }

    InitDataGeography(*pDG, blockNum);

    pUploadFileEvent->mDG.reset(pDG);

    
    int sendEventNum = 0;
    u64_t leftSize = tempStat.st_size;
    u64_t countSize = 0;
    for (int i = 0; i < blockNum; i++)
    {
        CUploadBlockEvent *pUploadBlockEvent = new CUploadBlockEvent(pUploadFileEvent, i);
        if (pUploadBlockEvent == NULL)
        {
            LOG_ERROR("Failed to alloc pUploadBlockEvent for %s:%d",
                pLocalFile, i);
            pUploadFileEvent->mError = ENOMEM;
            pUploadFileEvent->mErrMsg = "Failed to alloc pUploadBlockEvent for " + pUploadFileEvent->mFileName;

            break;
        }

        BlockGeography &bg = pDG->mBlocks[i];

        bg.mOffset = mBlockSize * i;
        bg.mLen    = min(mBlockSize, leftSize);
        bg.mStatus = 0;

        pUploadFileEvent->mBlockEvents.insert(pUploadBlockEvent);

        this->addEvent(pUploadBlockEvent);

        sendEventNum++;
        leftSize -= bg.mLen;
    }


    if (sendEventNum == 0)
    {
        pUploadFileEvent->done();
        
    }
    
    return ;
}

void 
ClientDfsStage::UploadBlock(StageEvent *event)
{
    int ret = STATUS_SUCCESS;
    
    CUploadBlockEvent *pUploadBlockEvent  = dynamic_cast<CUploadBlockEvent *>(event);

    ClientUploadEvent *pUploadFileEvent   = (ClientUploadEvent *)pUploadBlockEvent->mFileEvent;

    DataGeography     &dataGeography      = *pUploadFileEvent->mDG.get();

    BlockGeography    &blockGeography     = dataGeography.mBlocks[pUploadBlockEvent->mIndex];

    if (dataGeography.mBlockNum == 1)
    {
        ret = GlobalDfsInstance().UploadFile(pUploadFileEvent->mFileName.c_str(),
            blockGeography.mBlockId);
    }
    else
    {
        ret = GlobalDfsInstance().UploadFilePart(pUploadFileEvent->mFileName.c_str(),
            blockGeography.mOffset, blockGeography.mLen, blockGeography.mBlockId);
    }


    if (ret)
    {
        LOG_ERROR("Failed to download to part file %s, offset:%llu, rc:%d:%s",
            pUploadFileEvent->mFileName.c_str(), blockGeography.mOffset,
            ret, strerror(ret));

        pUploadFileEvent->mError = ret;
        pUploadFileEvent->mErrMsg= "Failed to download to part file " + 
            pUploadFileEvent->mFileName + 
            " failed in " + blockGeography.mBlockId;
        // the left operation is same as good operation
        
    }

    pUploadFileEvent->CheckFinished((StageEvent *)pUploadBlockEvent);

    pUploadBlockEvent->done();

    return ;
}

void
ClientDfsStage::DeleteFile(StageEvent *event)
{
    int ret = STATUS_SUCCESS;
    
    ClientDeleteEvent *pDeleteFileEvent = dynamic_cast<ClientDeleteEvent *>(event);

    if (pDeleteFileEvent->mBlockValue.empty() == true)
    {
        pDeleteFileEvent->done();
        return ;
    }

    if (pDeleteFileEvent->mType == ADFS_DATA_GEOG_SIMPLE)
    {
        s64_t fileSize = 0;
        ret = GlobalDfsInstance().DeleteFile(pDeleteFileEvent->mBlockValue.c_str(), fileSize);
        if (ret)
        {
            LOG_ERROR("Failed to delete file %s, rc:%d:%s",
               pDeleteFileEvent->mBlockValue.c_str(), ret, strerror(ret) );

            pDeleteFileEvent->mError  = ret;
            pDeleteFileEvent->mErrMsg =  "Failed to delete file " +  pDeleteFileEvent->mBlockValue;

            pDeleteFileEvent->done();
            return ;
        }

        pDeleteFileEvent->done();
        return ;
    }

    DataGeography      *pDGMeta = NULL;
    int                 dgLen = 0;

    ret = GlobalDGCache()->GetKeyV(pDeleteFileEvent->mBlockValue.c_str(), 
        (char **)&pDGMeta, &dgLen, META_READ);
    if (ret)
    {
        LOG_ERROR("Failed to get DataGeography of %s, rc:%d:%s",
                pDeleteFileEvent->mBlockValue.c_str(), ret, strerror(ret));

        pDeleteFileEvent->mError = ret;
        pDeleteFileEvent->mErrMsg = "Failed to get DataGeography of " + pDeleteFileEvent->mBlockValue;

        pDeleteFileEvent->done();

        return ;
    }

    DataGeography *pDG = (DataGeography *)new char[dgLen];
    memcpy(pDG, pDGMeta, dgLen);

    GlobalDGCache()->PutKeyV(pDeleteFileEvent->mBlockValue.c_str(), false, META_READ);

    pDeleteFileEvent->mDG.reset(pDG);
    

    int sendEventNum = 0;
    for(int i = 0; i < pDG->mBlockNum; i++)
    {
        CDeleteBlockEvent *pDeleteBlockEv = new CDeleteBlockEvent(pDeleteFileEvent, i);
        if (pDeleteBlockEv == NULL)
        {
            LOG_ERROR("Failed to alloc memory for CDownloadBlockEvent %s:%d",
                pDeleteFileEvent->mBlockValue.c_str(), i);

            pDeleteFileEvent->mError = ENOMEM;
            pDeleteFileEvent->mErrMsg = "Failed to alloc memory for CDownloadBlockEvent "  + 
                pDeleteFileEvent->mBlockValue;
            break;
            
        }

        pDeleteFileEvent->mBlockEvents.insert((StageEvent *)pDeleteBlockEv);

        this->addEvent(pDeleteBlockEv);

        sendEventNum++;
    }

    if (sendEventNum == 0)
    {
        pDeleteFileEvent->done();
    }

    return ;
}

void
ClientDfsStage::DeleteBlock(StageEvent *event)
{
    CDeleteBlockEvent *pDeleteBlockEvent = dynamic_cast<CDeleteBlockEvent * >(event);

    ClientDeleteEvent *pDeleteFileEvent  = (ClientDeleteEvent *)pDeleteBlockEvent->mFileEvent;

    DataGeography       &dataGeography   = *pDeleteFileEvent->mDG.get();

    BlockGeography      &blockGeography  = dataGeography.mBlocks[pDeleteBlockEvent->mIndex];

    s64_t fileSize = 0;
    int ret = GlobalDfsInstance().DeleteFile(blockGeography.mBlockId, fileSize);
    if (ret)
    {
        LOG_ERROR("Failed to delete %s, rc:%d:%s",
            blockGeography.mBlockId, ret, strerror(ret));

        pDeleteFileEvent->mError = ret;
        pDeleteFileEvent->mErrMsg= "Failed to delete  " + 
            pDeleteFileEvent->mBlockValue + 
            " failed in " + blockGeography.mBlockId;
        // the left operation is same as good operation
        
    }
    
    pDeleteFileEvent->CheckFinished((StageEvent *)pDeleteBlockEvent);

    pDeleteBlockEvent->done();

    return ;
}

