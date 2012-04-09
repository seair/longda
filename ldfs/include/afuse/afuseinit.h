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


#ifndef _AFUSE_INIT_H_
#define _AFUSE_INIT_H_

#include <vector>
#include <string>

int  Init();
void Cleanup();
void Restart(std::string cause);
void Finalize();
void Usage();

extern int gADFS;


//client seda macro
#define CLIENT_STAGE_NAME           "ClientStage"
#define META_DATA_STAGE_NAME        "MetaDataStage"
#define CLIENT_DFS_STAGE_NAME       "ClientDfsStage"
#endif  // _REPORT_INIT_H_

