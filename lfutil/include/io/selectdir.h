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
 * selectdir.h
 *
 *  Created on: May 8, 2012
 *      Author: Longda Feng
 */

#ifndef SELECTDIR_H_
#define SELECTDIR_H_

#include <string>

class CSelectDir
{
public:
    std::string select() {return std::string("");};
    void setBaseDir(std::string baseDir) {};
};


#endif /* SELECTDIR_H_ */
