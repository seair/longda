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

#include "metadatastage.h"
#include "clientdfstage.h"

#include "clientevent.h"
#include "metadataevent.h"
#include "clientdfsevent.h"


//! Constructor
MetaDataStage::MetaDataStage(const char* tag) :
    Stage(tag)
{
}


//! Destructor
MetaDataStage::~MetaDataStage()
{
}

//! Parse properties, instantiate a stage object
Stage*
MetaDataStage::makeStage(const std::string& tag)
{
    MetaDataStage* retValue = new MetaDataStage(tag.c_str());
    if( retValue == NULL )
    {
        LOG_ERROR("new MetaDataStage failed");
        return NULL;
    }
    retValue->setProperties();
    return retValue;
}

//! Set properties for this object set in stage specific properties
bool
MetaDataStage::setProperties()
{
    
    return true;
}

//! Initialize stage params and validate outputs
bool
MetaDataStage::initialize()
{
    LOG_TRACE("Enter");
    
    std::list<Stage*>::iterator stgp = nextStageList.begin();
    mClientDfsStage = *(stgp++);

    ClientDfsStage *clientDfsStage = NULL;
    clientDfsStage = dynamic_cast<ClientDfsStage *>(mClientDfsStage);
    ASSERT( clientDfsStage, "The next stage isn't ClientDfsStage" );

    LOG_TRACE("Exit");
    return true;
}

//! Cleanup after disconnection
void
MetaDataStage::cleanup()
{
    LOG_TRACE("Enter");
    
   
    LOG_TRACE("Exit");
}

void
MetaDataStage::handleEvent(StageEvent* event)
{
    LOG_TRACE("Enter\n");

    if (dynamic_cast<ClientTestEvent *>(event))
    {
        handleClientTest(event);
        return ;
    }
    
    LOG_TRACE("Exit\n");
    return ;
}


void 
MetaDataStage::callbackEvent(StageEvent* event, CallbackContext* context)
{
    LOG_TRACE("Enter\n");

    if (dynamic_cast<ClientTestEvent *>(event) )
    {
        handleClientTestCb(event);
        return ;
    }
    LOG_TRACE("Exit\n");
    return ;
}

void
MetaDataStage::handleClientTest(StageEvent *event)
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

    mClientDfsStage->addEvent(clientTestEvent);

    return ;
}

void
MetaDataStage::handleClientTestCb(StageEvent *event)
{
    ClientTestEvent *clientTestEvent = dynamic_cast<ClientTestEvent *>(event);

    LOG_INFO("%s Handle ClientTestEvent", __FUNCTION__);

    clientTestEvent->done();
    LOG_INFO("Finish handle");
}

