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

#include <iostream>


#include "util.h"
#include "log.h"
#include "afuseinit.h"

#include "fdfs.h"
#include "fdht.h"
#include "metacache.h"

#include "stagefactory.h"
#include "clientstage.h"
#include "metadatastage.h"
#include "clientdfstage.h"

#include "sedaconfig.h"


void Usage()
{
   std::cerr
      << "\n"
      << "usage: [options] \n"
      << "\n"
      << "options:\n"
      << "   -f <filename>    = config filename\n"
      << "   -d               = run as a daemon\n"
      << "   -o <filename>    = file to redirect stdout to if daemon\n"
      << "   -e <filename>    = file to redirect stderr to if daemon\n";

}

int gADFS = 0;

int Init()
{
    static StageFactory ClientFactory(CLIENT_STAGE_NAME, &ClientStage::makeStage);
    static StageFactory MetaDataFactory(META_DATA_STAGE_NAME, &MetaDataStage::makeStage);
    static StageFactory ClientDfsFactory(CLIENT_DFS_STAGE_NAME, &ClientDfsStage::makeStage);
    
    int rc = UtilInit(GlobalConfigPath());
    if (rc)
    {
        std::cerr << "Failed to Init common component" << std::endl;
        return rc;
    }
    LOG_INFO("\n\n ***************Begin Init ************* \n\n");

    rc = InitDfs();
    if (rc)
    {
        LOG_ERROR("Failed to init DFS");
        return rc;
    }
    LOG_INFO("Successfully init DFS client");

    rc = InitDht();
    if (rc)
    {
        LOG_ERROR("Failed to init DHT");
        return rc;
    }
    LOG_INFO("Successfully init DHT client");

    rc = GlobalMetaCache()->Init();
    if (rc)
    {
        LOG_ERROR("Failed to init metacache");
        return rc;
    }
    LOG_INFO("Successfully init metacache");

    rc = GlobalDGCache()->Init();
    if (rc)
    {
        LOG_ERROR("Failed to init datageography cache");
        return rc;
    }
    LOG_INFO("Successfully init datageography cache");

    std::string adfsKey("DO_ADFS");
    std::string adfsValue;

    adfsValue = theGlobalProperties()->GetParamValue(adfsKey, DEFAULT_SESSION);
    if (adfsValue.size())
    {
        Xlate::strToVal(adfsValue, gADFS);
    }

    //handle function
    SetSigFunc(SIGUSR2, Monitor);

    LOG_INFO("Successfully init");

    return STATUS_SUCCESS;
}

void Restart(const std::string cause)
{
    LOG_WARN("Shutting down cause = %s", cause.c_str());
    
    pid_t pid = getpid();

    kill(pid, SIGUSR1);
    return ;
}


void Finalize()
{
    LOG_TRACE("Enter");
    
    LOG_WARN("Starting shutdown");

    delete GlobalDGCache();

    delete GlobalMetaCache();

    UtilFinalize();

    delete GlobalConfigPath();
}

