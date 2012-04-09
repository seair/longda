// __CR__
// Copyright (c) 2008-2011 Longda Corporation
// All Rights Reserved
// 
// This software contains the intellectual property of Longda Corporation
// or is licensed to Longda Corporation from third parties.  Use of this 
// software and the intellectual property contained therein is expressly
// limited to the terms and conditions of the License Agreement under which 
// it is provided by or on behalf of Longda.
// __CR__

/**
 * @ author: hustjackie@gmail.com
 * @ date:  2010/04/01
 * @ func:  provide project version defines
 */

#ifndef _VERSION_H_
#define _VERSION_H_

#define MAIJOR_VER           1
#define MINOR_VER            0
#define PATCH_VER            0
#define OTHER_VER            1

#define VERSION_STR          "1.0.0.1"
#define VERSION_NUM          (MAIJOR_VER << 24  | \
                              MINOR_VER  << 16  | \
                              PATCH_VER  << 8   | \
                              OTHER_VER)

#endif
