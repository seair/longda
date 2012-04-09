// __CR__
// Copyright (c) 2008-2012 LongdaFeng
// All Rights Reserved
//
// This software contains the intellectual property of LongdaFeng
// or is licensed to LongdaFeng from third parties.  Use of this
// software and the intellectual property contained therein is
// expressly limited to the terms and conditions of the License Agreement
// under which it is provided by or on behalf of LongdaFeng.
// __CR__

/*
 * CTestStage.cpp
 *
 *  Created on: Apr 5, 2012
 *      Author: Longda Feng
 */

#include "teststage.h"
#include "conf/ini.h"
#include "lang/lstring.h"
#include "seda/timerstage.h"

#define TEST_STAGE_FREQUENCE 5

//! Constructor
CTestStage::CTestStage(const char* tag) :
        Stage(tag)
{
}

//! Destructor
CTestStage::~CTestStage()
{
}

//! Parse properties, instantiate a stage object
Stage* CTestStage::makeStage(const std::string& tag)
{
    CTestStage* stage = new CTestStage(tag.c_str());
    if (stage == NULL)
    {
        LOG_ERROR("new CTestStage failed");
        return NULL;
    }
    stage->setProperties();
    return stage;
}

//! Set properties for this object set in stage specific properties
bool CTestStage::setProperties()
{
    std::string stageNameStr(stageName);
    std::map<std::string, std::string> section = theGlobalProperties()->get(stageNameStr);

    std::map<std::string, std::string>::iterator it;

    std::string key;

    key = "Frequence";
    it = section.find(key);
    if (it == section.end())
    {
        mFrequence = TEST_STAGE_FREQUENCE;
    }
    else
    {
        CLstring::strToVal(it->second, mFrequence);
    }

    return true;
}

//! Initialize stage params and validate outputs
bool CTestStage::initialize()
{
    LOG_TRACE("Enter");

    std::list<Stage*>::iterator stgp = nextStageList.begin();
    mTimerStage = *(stgp++);

    ASSERT(dynamic_cast<TimerStage *>(mTimerStage),
            "The next stage isn't TimerStage");

    LOG_TRACE("Exit");
    return true;
}

//! Cleanup after disconnection
void CTestStage::cleanup()
{
    LOG_TRACE("Enter");

    LOG_TRACE("Exit");
}

void CTestStage::handleEvent(StageEvent* event)
{
    LOG_TRACE("Enter\n");

    handleTestEvent(event);

    LOG_TRACE("Exit\n");
    return;
}

void CTestStage::callbackEvent(StageEvent* event, CallbackContext* context)
{
    LOG_TRACE("Enter\n");

    handleCb(event);

    LOG_TRACE("Exit\n");
    return;
}

void CTestStage::handleTestEvent(StageEvent *event)
{
    LOG_INFO("Handle TestEvent");

    CompletionCallback *cb = new CompletionCallback(this, NULL);
    if (cb == NULL)
    {
        LOG_ERROR("Failed to new callback");

        event->done();

        return;
    }

    event->pushCallback(cb);

    TimerRegisterEvent *tmEvent = new TimerRegisterEvent(event,
            mFrequence * USEC_PER_SEC);

    mTimerStage->addEvent(tmEvent);

    return;
}

void CTestStage::handleCb(StageEvent *event)
{

    static int i = 0;

    LOG_INFO("Handle Callback %d", i++);

    addEvent(event);

    LOG_DEBUG("Finish handle");
}

