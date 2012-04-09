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



#ifndef __CLIENT_STAGE__
#define __CLIENT_STAGE__

#include "util.h"
#include "log.h"
#include "mutex.h"

#include "stage.h"
#include "stageevent.h"
#include "callback.h"


class ClientStage : public Stage
{
public:
    ~ClientStage();
    static Stage*    makeStage(const std::string& tag);
    
protected:
    //common function
    ClientStage(const char* tag);
    bool             setProperties();

    bool             initialize();
    void             cleanup();
    void             handleEvent(StageEvent* event);
    void             callbackEvent(StageEvent* event, CallbackContext* context);

protected:
    
    //Hanlde ClientTestEvent
    void handleTestEvent(StageEvent *event);

    void handleDfsEvent(StageEvent *event);

    void handleCb(StageEvent *event);
    
private:
    Stage                    *mMetaDataStage;
    Stage                    *mDfsStage;
};

void Monitor(int i);


#endif //__CLIENT_STAGE__

