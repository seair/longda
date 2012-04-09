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
 * lnet.cpp
 *
 *  Created on: Mar 26, 2012
 *      Author: Longda Feng
 */

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "net/lnet.h"
#include "trace/log.h"

int getHostname(const char *ip, std::string& hostname)
{
    if (ip == NULL || ip[0] == '\0')
    {
        char name[256] =  { 0 };
        if (gethostname(name, sizeof(name)) != 0)
        {
            std::cerr << "Failed to get local hostname" << SYS_OUTPUT_ERROR << std::endl;
            return -1;
        }
        else
        {
            hostname = name;
            return 0;
        }
    }
    else
    {
        int rc;
        struct in_addr addr;
        rc = inet_aton(ip, &addr);
        if (rc == 0)
        {
            std::cerr << "Failed to get " << ip << "  hostname "
                    << SYS_OUTPUT_ERROR <<std::endl;
            return -1;
        }

        struct hostent hostent_struct, *record = NULL;
        char buff[4096];
        int err;
        rc = gethostbyaddr_r(&addr, sizeof(addr), AF_INET, &hostent_struct,
                buff, sizeof(buff), &record, &err);
        if (rc)
        {
            std::cerr << "Failed to get " << ip << "  hostname "
                    << SYS_OUTPUT_ERROR <<std::endl;
            return 0;
        }

        hostname = hostent_struct.h_name;
        return 0;
    }

    return -1;
}


//g_max_connections = iniGetIntValue(NULL, "max_connections", \                                                    |||     sync_stat_file_lock
//1151                 &iniContext, DEFAULT_MAX_CONNECTONS);                                                                    ||
//1152         if (g_max_connections <= 0)                                                                                      ||-   function
//1153         {                                                                                                                |||     get_storage_stat_filena
//1154             g_max_connections = DEFAULT_MAX_CONNECTONS;                                                                  |||     storage_write_to_fd
//1155         }                                                                                                                |||     storage_open_stat_file
//1156         if ((result=set_rlimit(RLIMIT_NOFILE, g_max_connections)) != 0)                                                  |||     storage_close_stat_file
//1157         {                                                                                                                |||     storage_write_to_stat_f
//1158             break;                                                                                                       |||     storage_write_to_sync_i
//1159         }
