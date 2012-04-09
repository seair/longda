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



#ifndef __CLIENT_UPLOAD_STAGE__
#define __CLIENT_UPLOAD_STAGE__

#include "util.h"
#include "log.h"
#include "mutex.h"

#include "stage.h"
#include "stageevent.h"
#include "callback.h"



class ClientDfsStage : public Stage
{
public:
    ~ClientDfsStage();
    static Stage*    makeStage(const std::string& tag);

protected:
    //common function
    ClientDfsStage(const char* tag);
    bool             setProperties();

    bool             initialize();
    void             cleanup();
    void             handleEvent(StageEvent* event);
    void             callbackEvent(StageEvent* event, CallbackContext* context);

protected:
    
    //Hanlde ClientTestEvent
    void handleClientTest(StageEvent *event);
    void handleClientTestCb(StageEvent *event);

    void DownloadFile(StageEvent *event);
    void DownloadBlock(StageEvent *event);

    void UploadFile(StageEvent *event);
    void UploadBlock(StageEvent *event);

    void DeleteFile(StageEvent *event);
    void DeleteBlock(StageEvent *event);
    
private:

    u64_t        mBlockSize;
    Stage       *mTimerStage;
};

#endif //__CLIENT_UPLOAD_STAGE__

