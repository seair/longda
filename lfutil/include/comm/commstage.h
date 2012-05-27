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
 * commstage.h
 *
 *  Created on: Apr 25, 2012
 *      Author: Longda Feng
 */

#ifndef COMMSTAGE_H_
#define COMMSTAGE_H_

#include <string>
#include <map>

#include "seda/stage.h"

#include "defs.h"
#include "trace/log.h"
#include "os/mutex.h"

#include "seda/stage.h"
#include "seda/stageevent.h"
#include "seda/callback.h"

#include "lang/serializable.h"
#include "io/selectdir.h"

#include "comm/message.h"
#include "comm/request.h"
#include "comm/response.h"
#include "comm/packageinfo.h"
#include "comm/commevent.h"
#include "comm/commdataevent.h"

#include "net/conn.h"
#include "net/net.h"
#include "net/netex.h"
#include "net/endpoint.h"

/**
 *
 */
class CommStage : public Stage
{
public:
    ~CommStage();
    static Stage*    makeStage(const std::string& tag);

    /**
     * Send CommEvent with request
     */
    void sendRequest(CommEvent* cev);

    /**
     * Send CommEvent with response
     */
    void sendResponse(CommEvent* cev);

    void setDeserializable(Deserializable *deserializer);

    void setSelectDir(CSelectDir *selectDir);

    Stage* getNextStage();

    void clearSelector(int sock);

protected:
    //common function
    CommStage(const char* tag);
    bool             setProperties();

    bool             initialize();
    void             cleanup();
    void             handleEvent(StageEvent* event);
    void             callbackEvent(StageEvent* event, CallbackContext* context);

protected:

    /**
     * cleanup when send request CommEvent failed
     */
    void cleanupFailedReq(MsgDesc &md, CommEvent* cev, Conn *conn, CommEvent::status_t errCode);

    /**
     * cleanup when send responsne CommEvent failed
     */
    void cleanupFailedResp(CommEvent* cev, CommEvent::status_t errCode,
            IoVec** iovs, int iovsNum);



private:

    bool                        mServer;
    Net                        *mNet;

    Stage                      *mNextStage;

    u32_t                       mSendCounter;    //!< a counter for the messages
    pthread_mutex_t             mCounterMutex;   //!< counter lock
};





#endif /* COMMSTAGE_H_ */
