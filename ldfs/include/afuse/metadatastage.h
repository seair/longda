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



#ifndef __CLIENT_CACHE_STAGE__
#define __CLIENT_CACHE_STAGE__

#include "util.h"
#include "log.h"
#include "mutex.h"

#include "stage.h"
#include "stageevent.h"
#include "callback.h"



class MetaDataStage : public Stage
{
public:
    ~MetaDataStage();
    static Stage*    makeStage(const std::string& tag);
    
protected:
    //common function
    MetaDataStage(const char* tag);
    bool             setProperties();

    bool             initialize();
    void             cleanup();
    void             handleEvent(StageEvent* event);
    void             callbackEvent(StageEvent* event, CallbackContext* context);

protected:
    
    //Hanlde ClientTestEvent
    void handleClientTest(StageEvent *event);
    void handleClientTestCb(StageEvent *event);
    
private:
    Stage                    *mClientDfsStage;
};

#endif //__CLIENT_CACHE_STAGE__

