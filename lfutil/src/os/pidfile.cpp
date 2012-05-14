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
 * @ func:  provide project pid operations
 */


#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <paths.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <assert.h>

#include "os/pidfile.h"
#include "trace/log.h"

static std::ostringstream path;

int writePidFile(const char *progName)
{
    assert(progName);
    std::ofstream ostr;
    int rv = 1;
    //path << _PATH_VARRUN << progName << ".pid" << std::ends;
    path << _PATH_TMP << progName << ".pid" << std::ends;
    ostr.open(path.str().c_str(), std::ios::trunc);
    if (ostr.good()) {
        ostr << getpid() << std::endl;
        ostr.close();
        rv = 0;
    }
    else {
        rv = errno;
        std::cerr << "error opening PID file " << path.str()
             << SYS_OUTPUT_ERROR << std::endl;
    }

    return rv;
}

void removePidFile(void)
{
    if (!path.str().empty()) {
        unlink(path.str().c_str());
        path.clear();
        path.str("");
    }
    return;
}
