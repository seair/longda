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
 * CTestStage.h
 *
 *  Created on: Apr 5, 2012
 *      Author: Longda Feng
 */

#ifndef CTESTSTAGE_H_
#define CTESTSTAGE_H_

#include "defs.h"
#include "trace/log.h"
#include "os/mutex.h"

#include "seda/stage.h"
#include "seda/stageevent.h"
#include "seda/callback.h"

/**
 *
 */
class CTestStage : public Stage
{
public:
    ~CTestStage();
    static Stage*    makeStage(const std::string& tag);

protected:
    //common function
    CTestStage(const char* tag);
    bool             setProperties();

    bool             initialize();
    void             cleanup();
    void             handleEvent(StageEvent* event);
    void             callbackEvent(StageEvent* event, CallbackContext* context);

protected:
    void handleTestEvent(StageEvent *event);

    void handleCb(StageEvent *event);
private:
    int                       mFrequence;
    Stage                    *mTimerStage;
};

#endif /* CTESTSTAGE_H_ */
