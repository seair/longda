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
 * test.cpp
 *
 *  Created on: Apr 5, 2012
 *      Author: Longda Feng
 */


#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <string>
#include <iostream>

#include "defs.h"
#include "os/lprocess.h"
#include "os/pidfile.h"
#include "os/lsignal.h"
#include "trace/log.h"


#include "io/rollselectdir.h"

#include "seda/stagefactory.h"
#include "seda/stage.h"
#include "seda/stageevent.h"
#include "seda/sedaconfig.h"

#include "comm/commstage.h"


#include "teststage.h"
#include "simpledeserializer.h"
#include "triggertestevent.h"


#include "linit.h"

void usage()
{
    std::cout << "test liblutil.so" << std::endl;
}

const char TEST_STAGE_NAME[] = "TestStage";
const char COMM_STAGE_NAME[] = "CommStage";

int setTestSeda()
{
    static StageFactory testThreadFactory(TEST_STAGE_NAME, &CTestStage::makeStage);

    return 0;
}


void startTest()
{
    static Stage *testStage = theSedaConfig()->getStage(TEST_STAGE_NAME);
    if (testStage == NULL)
    {
        LOG_ERROR("Failed to get %s stage", TEST_STAGE_NAME);
        return;
    }

    TriggerTestEvent *event = new TriggerTestEvent(10);
    if (event == NULL)
    {
        LOG_ERROR("Failed to alloc memory for StageEvent");
        return;
    }

    testStage->addEvent(event);


    LOG_INFO("Successfully add one event to %s", TEST_STAGE_NAME);

    return;
}

void setDeserializer()
{
    static Stage *commStage = theSedaConfig()->getStage(COMM_STAGE_NAME);

    Deserializable *deserializer = new CSimpleDeserializer();

    ((CommStage *)commStage)->setDeserializable(deserializer);
}

void setSelectDir()
{
    static Stage *commStage = theSedaConfig()->getStage(COMM_STAGE_NAME);

    CSelectDir *selector = new CRollSelectDir();

    std::string baseDir = theGlobalProperties()->get("BaseDataDir", "./", "Default");

    selector->setBaseDir(baseDir);

    ((CommStage *)commStage)->setSelectDir(selector);
}


int init()
{
    int rc = initUtil(theProcessParam());
    if (rc)
    {
        cleanupUtil();
        return STATUS_FAILED_INIT;
    }

    rc = setTestSeda();
    if (rc)
    {
        cleanupUtil();
        return STATUS_FAILED_INIT;
    }

    /**
     * start seda
     */
    rc = initSeda(theProcessParam());
    if (rc)
    {
        cleanupUtil();
        return STATUS_FAILED_INIT;
    }

    setDeserializer();
    setSelectDir();

    return 0;
}

int main(int argc, char** argv)
{
    // Initializations

    std::string processName = getProcessName(argv[0]);

    theProcessParam()->init(processName);


    // Process args
    int rc = STATUS_SUCCESS;
    int opt;
    extern char* optarg;
    while((opt = getopt(argc, argv, "ds:f:o:e:h")) > 0) {
        switch(opt) {
        case 'f':
            theProcessParam()->mProperties = optarg;
            break;
        case 'o':
            theProcessParam()->mStdOut     = optarg;
            break;
        case 'e':
            theProcessParam()->mStdErr     = optarg;
            break;
        case 'd':
            theProcessParam()->mDemon = true;
            break;
        case 'h':
        default:
            usage();
            return STATUS_INVALID_PARAM;
        }
    }

    // Run as daemon if daemonization requested

    if (theProcessParam()->mDemon)
    {
        rc = daemonizeService(theProcessParam()->mStdOut.c_str(),
                theProcessParam()->mStdOut.c_str());
        if (rc != 0)
        {
            return STATUS_FAILED_INIT;
        }
        rc = writePidFile(processName.c_str());
        if (rc != 0)
        {
            return STATUS_FAILED_INIT;
        }
    }

    // Block interrupt signals before creating child threads.
    sigset_t signal_set, oset;
    blockSignalsDefault(&signal_set, &oset);

    rc = init();
    if (rc)
    {
        std::cerr << "Failed to init" << std::endl;
        return rc;
    }

    startTest();

    // wait interrupt signals
    int signal_number = -1;
    waitForSignals(&signal_set, signal_number);

    cleanupUtil();
    if (SIGUSR1 != signal_number)
    {
        removePidFile();
    }

    return STATUS_SUCCESS;
}

