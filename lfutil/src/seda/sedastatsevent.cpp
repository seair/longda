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



/// 
/// @file sedastatsevent.cxx
/// @brief Event for the Seda stats stage
/// @author Longda
/// @date 2007-10-18
/// 

#include "seda/sedastatsevent.h"
#include "seda/sedastats.h"

/// @brief Constructor
/// 
/// @param[in] identifier   identify the stat being collected
/// @param[in] time         time taken for stat
SedaStatsEvent::SedaStatsEvent(SedaStats::sedaStatsCategory_t      category,
                               const SedaStats::StatsId&           statId,
                               bool persistent) :
    StageEvent(),
    _category(category),
    _statId(statId),
    _statTime(0),
    _statCount(0),
    _timeCollected(false),
    _countCollected(false),
    _resetCount(false),
    _persistent(persistent)
{
}

/// @brief Return the category for the stat being recorded
/// 
/// @return category for the stat being collected
SedaStats::sedaStatsCategory_t SedaStatsEvent::getCategory() const
{
    return _category;
}

/// @brief Return the ID for the stat being recorded
/// 
/// @return id for the stat being collected
const SedaStats::StatsId& SedaStatsEvent::getStatID() const
{
    return _statId;
}

bool SedaStatsEvent::hasTime() const
{
    return _timeCollected;
}

/// @brief Return if a count was collected in this event
/// 
/// @return boolean
bool SedaStatsEvent::hasCount() const
{
    return _countCollected;
}

/// @brief Return the time stored in the event
/// 
/// @return the time to be recorded
unsigned long long SedaStatsEvent::getTime() const
{
    return _statTime;
}

void SedaStatsEvent::setTime(unsigned long long t)
{
    _timeCollected = true;
    _statTime = t;
}

/// @brief Return the count set in the event
/// 
/// @return count
int SedaStatsEvent::getStatCount() const
{
    return _statCount;
}

// Set stat count
void SedaStatsEvent::setStatCount(int count)
{
    _countCollected = true;
    _statCount = count;
}

void SedaStatsEvent::resetCount(bool flag)
{
    _resetCount = flag;
}

//Reset Count
bool SedaStatsEvent::resetCount() const
{
    return _resetCount;
}

//is persistent
bool SedaStatsEvent::isPersistent() const
{
    return _persistent;
}
