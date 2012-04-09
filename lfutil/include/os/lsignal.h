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
 * lsignal.h
 *
 *  Created on: Mar 26, 2012
 *      Author: Longda Feng
 */

#ifndef LSIGNAL_H_
#define LSIGNAL_H_


#include <signal.h>

//! Default function that blocks signals.
/**
 * Now it blocks SIGINT, SIGTERM, and SIGUSR1
 */
void blockSignalsDefault(sigset_t *signal_set, sigset_t *old_set);
//! Default function that unblocks signals.
/**
 * It unblocks SIGINT, SIGTERM,and SIGUSR1.
 */
void unBlockSignalsDefault(sigset_t *signal_set, sigset_t *old_set);

void waitForSignals(sigset_t *signal_set, int& sig_number);

// Set signal handling function
/**
 * handler function
 */
typedef void (*sighandler_t)(int);
void setSignalHandlingFunc(sighandler_t func);
void setSigFunc(int sig, sighandler_t func);

void waitForSignals(sigset_t *signal_set);

#endif /* LSIGNAL_H_ */
