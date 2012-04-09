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


#include "net/iovec.h"

//! Implementaion of IoVec
/**
 * @file
 * @author Longda
 * @date   5/20/07
 * IoVec represents the unit of work the network layer (Net class) performs
 * when sending or receiving data. Messages are broken into one or more IoVec's
 * which are submitted to the network layer for processing. 
 */

IoVec::IoVec(alloc_t alloc) : 
    base(0), 
    size(0), 
    xferred(0), 
    callback(0), 
    cbParam(0),
    alloc(alloc) {}

IoVec::IoVec(void *base, size_t size, alloc_t alloc) : 
    base(base), 
    size(size), 
    xferred(0),
    callback(0),
    cbParam(0),
    alloc(alloc) {}

IoVec::IoVec(void *base, size_t size, 
             callback_t callback, void *param, alloc_t alloc): 
    base(base), 
    size(size), 
    xferred(0),
    callback(callback),
    cbParam(param),
    alloc(alloc) {}

IoVec::IoVec(vec_t *vec, alloc_t alloc) : 
    base(vec->base), 
    size(vec->size), 
    xferred(0),
    callback(0),
    cbParam(0),
    alloc(alloc) {}

IoVec::~IoVec() {}

void
IoVec::setBase(void *base) {this->base = base;}

void
IoVec::setSize(size_t size) {this->size = size;}

void
IoVec::setXferred(size_t xferred) {this->xferred = xferred;}

void
IoVec::setVec(void *base, size_t size) 
{
    this->base = base; 
    this->size = size;
    this->xferred = 0;
}

void
IoVec::setVec(vec_t *vec) 
{
    this->base = vec->base; 
    this->size = vec->size;
    this->xferred = 0;
}

void
IoVec::setCallback(callback_t callback, void *param)
{
    this->callback = callback;
    this->cbParam = param;
}

IoVec::callback_t
IoVec::getCallback() {return callback;}

void *
IoVec::getCallbackParam() {return cbParam;} 

void
IoVec::incXferred(size_t inc) {xferred += inc;}

void
IoVec::reset() {xferred = 0; }

void *
IoVec::getBase() {return this->base;}

size_t
IoVec::getSize() {return this->size;}

size_t
IoVec::getXferred() {return this->xferred;}

IoVec::vec_t
IoVec::getVec() {vec_t vec = {this->base, this->size}; return vec;}

void *
IoVec::curPtr() {return (void *)((char *)this->base + this->xferred);} 

size_t
IoVec::remain() {return this->size - this->xferred;}

bool
IoVec::done() {return this->xferred == this->size;}

bool
IoVec::started() {return this->xferred > 0;}

void
IoVec::setAllocType(alloc_t alloc) {this->alloc = alloc;}

IoVec::alloc_t
IoVec::getAllocType() {return this->alloc;}

