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

#include <errno.h>

#include "afuseinit.h"

#include "clientstage.h"
#include "metadatastage.h"
#include "clientdfstage.h"
#include "sedaconfig.h"

#include "clientevent.h"
#include "metadataevent.h"

void Monitor(int i)
{
    static Stage *clientStage = theSedaConfig()->getStage(CLIENT_STAGE_NAME);
    if (clientStage == NULL)
    {
        LOG_ERROR("Failed to get %s stage", CLIENT_STAGE_NAME);
        return ;
    }

    ClientTestEvent *clientEvent = new ClientTestEvent();
    if (clientEvent == NULL)
    {
        LOG_ERROR("Failed to alloc memory for clientevent");
        return ;
    }

    clientStage->addEvent(clientEvent);
    int rc = clientEvent->WaitProcessing();
    if (rc)
    {
        LOG_ERROR("Failed to handle ClientTestEvent");
        
    }
    else
    {
        LOG_INFO("Successfully handle ClientTestEvent");
    }

    clientEvent->done();

    return ;
        
}

int HandleClientEvent(ClientEvent *event, char** finishData)
{
    
    static Stage *clientStage = theSedaConfig()->getStage(CLIENT_STAGE_NAME);
    if (clientStage == NULL)
    {
        LOG_ERROR("Failed to get %s stage", CLIENT_STAGE_NAME);
        return STATUS_FAILED_INIT;
    }

    clientStage->addEvent(event);
    int rc = event->WaitProcessing();
    if (rc == STATUS_SUCCESS)
    {
        *finishData = (char *)event->GetFinishData();
    }

    event->done();

    return rc;
        
}

int ClientEvent::WaitProcessing()
{
    while (sem_wait(&mSem) && (errno == EINTR))
    {
        continue;
        
    }
    int err = mError;

    return err;
}

void ClientEvent::finish()
{
    sem_post(&mSem);
}

//! Constructor
ClientStage::ClientStage(const char* tag) :
    Stage(tag)
{
}


//! Destructor
ClientStage::~ClientStage()
{
}

//! Parse properties, instantiate a stage object
Stage*
ClientStage::makeStage(const std::string& tag)
{
    ClientStage* retValue = new ClientStage(tag.c_str());
    if( retValue == NULL )
    {
        LOG_ERROR("new ClientStage failed");
        return NULL;
    }
    retValue->setProperties();
    return retValue;
}

//! Set properties for this object set in stage specific properties
bool
ClientStage::setProperties()
{
    
    return true;
}

//! Initialize stage params and validate outputs
bool
ClientStage::initialize()
{
    LOG_TRACE("Enter");
    
    std::list<Stage*>::iterator stgp = nextStageList.begin();
    mMetaDataStage = *(stgp++);
    mDfsStage      = *(stgp++);

    ASSERT( dynamic_cast<MetaDataStage *>(mMetaDataStage), "The next stage isn't MetaDataStage" );
    ASSERT( dynamic_cast<ClientDfsStage *>(mDfsStage), "The next stage isn't ClientDfsStage" );

    LOG_TRACE("Exit");
    return true;
}

//! Cleanup after disconnection
void
ClientStage::cleanup()
{
    LOG_TRACE("Enter");
    
   
    LOG_TRACE("Exit");
}

void
ClientStage::handleEvent(StageEvent* event)
{
    LOG_TRACE("Enter\n");

    if (dynamic_cast<ClientEvent *>(event) == NULL)
    {
        LOG_ERROR("Get one non ClientEvent in ClientStage");
        event->done();
        return ;
    }

    if (dynamic_cast<ClientTestEvent *>(event))
    {
        handleTestEvent(event);
        return ;
    }
    else if (dynamic_cast<ClientDfsEvent *>(event))
    {
        handleDfsEvent(event);
        return ;
    }
    
    LOG_TRACE("Exit\n");
    return ;
}


void 
ClientStage::callbackEvent(StageEvent* event, CallbackContext* context)
{
    LOG_TRACE("Enter\n");

    if (dynamic_cast<ClientTestEvent *>(event))
    {
        handleCb(event);
        return ;
    }
    else if (dynamic_cast<ClientDfsEvent *>(event))
    {
        handleCb(event);
        return ;
    }
    
    LOG_TRACE("Exit\n");
    return ;
}

void
ClientStage::handleTestEvent(StageEvent *event)
{
    ClientTestEvent *clientTestEvent = dynamic_cast<ClientTestEvent *>(event);

    LOG_INFO("%s Handle ClientTestEvent", __FUNCTION__);

    CompletionCallback *cb = new CompletionCallback(this, NULL);
    if (cb == NULL)
    {
        LOG_ERROR("Failed to new callback");

        clientTestEvent->mError = ENOMEM;

        clientTestEvent->finish();

        return ;
    }

    clientTestEvent->pushCallback(cb);

    mMetaDataStage->addEvent(clientTestEvent);

    return ;
}

void
ClientStage::handleCb(StageEvent *event)
{
    ClientEvent *clientEvent = dynamic_cast<ClientEvent *>(event);

    LOG_DEBUG("%s Handle", __FUNCTION__);

    clientEvent->finish();

    LOG_DEBUG("Finish handle");
}

void
ClientStage::handleDfsEvent(StageEvent *event)
{
    ClientDfsEvent *clientDfsEvent = dynamic_cast<ClientDfsEvent *>(event);

    LOG_DEBUG("%s Handle ClientTestEvent", __FUNCTION__);

    CompletionCallback *cb = new CompletionCallback(this, NULL);
    if (cb == NULL)
    {
        LOG_ERROR("Failed to new callback");

        clientDfsEvent->mError = ENOMEM;

        clientDfsEvent->finish();

        return ;
    }

    clientDfsEvent->pushCallback(cb);

    mDfsStage->addEvent(clientDfsEvent);

    return ;
}

