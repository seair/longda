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

#ifndef __ADFS_SEDA_METRIC__ 
#define __ADFS_SEDA_METRIC__

#include <vector>
#include <deque>
#include <sstream>
#include <iostream>

//! An abstract class for seda metrics
class SedaMetric
{
public:
    virtual ~SedaMetric() {} ;
    virtual void dump(std::ostream& os) const = 0;
};

//! A class for maintaining historical seda metrics
template <class T>
class MetricHistory
{
public:
    //! constructor
    /**
     * @param[in] capacity   Maximum number of metrics can be kept
     */
    MetricHistory(size_t capacity) : max_num(capacity) {}

    //! add a new metric sample
    void add(const T& elem)
    {
        metric_history.push_back(elem);
        if(size() > capacity())
            metric_history.pop_front();
    }

    //! dump latest 'num' metrics
    void dump(std::ostream& os, size_t num) const
    {
        if(num > size()) num = size();

        for(size_t i = 0; i < num; ++i)
            metric_history[num - i - 1].dump(os);
    }

    //! return number of metric data available now
    size_t size() const { return metric_history.size(); }

    //! return max number of metric data that can be kept
    size_t capacity() const { return max_num; }

    //! clear all historical data
    void clear() { metric_history.clear(); }         
private:
    //! container of history metrics
    std::deque<T> metric_history;
    //! max number of metrics that can be kept
    size_t         max_num;
};


class SedaQueueInfo : public SedaMetric
{
public:
    //! constructor
    SedaQueueInfo();
    
    //! copy constructor
    SedaQueueInfo(const SedaQueueInfo& rhs);

    //! assignment operator
    SedaQueueInfo& operator = (const SedaQueueInfo&);

    //! destructor
    ~SedaQueueInfo() {}

    //! update queue info
    void update();

    //! dump queue info
    void dump(std::ostream& os) const;

private:
    static std::vector<std::string> stage_names;
    std::vector<int> queue_info;
};

#endif //__ADFS_SEDA_METRIC__
