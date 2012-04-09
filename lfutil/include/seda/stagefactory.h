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


#ifndef __STAGE_FACTORY__
#define __STAGE_FACTORY__

#include "seda/classfactory.h"
#include "seda/stage.h"

//! A class to construct arbitrary seda stage instances
/** 
 *  @author Longda
 *  @date   4/12/07
 *
 *  Instantiation of ClassFactory template for Stages
 */

class Stage;

typedef ClassFactory<Stage> StageFactory;

#endif  // __STAGE_FACTORY__


