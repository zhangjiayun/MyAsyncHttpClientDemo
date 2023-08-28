#ifndef _TIMER_H
#define _TIMER_H

#include <atomic>
#include <functional>
#include <stdint.h>
typedef std::function<void()> TimerHandler;

typedef int64_t TimerID;  //int64要引用stdint.h

//using TimerHandler = std::function<void(void* param)>;

class Timer
{
public:
    Timer(int64_t intervalMs, int32_t repeatCount, const TimerHandler& timerCallback);
    ~Timer() {};

    void run();

    bool isExpired();      //判断定时器是否到期
    TimerID getId() { return m_id; };
    int32_t getRepeatedTimes() { return m_repeatedTimes; };
    time_t getExpiredTime() const { return m_expiredTime; };
    void setExpiredTime(time_t itime) { m_expiredTime = itime; };

public:
    //生成一个唯一的id
    static TimerID generateId();

private:
    //定时器id基准值，初始值为0
    static std::atomic<TimerID> s_initialId;

private:
    //定时器的id，唯一标识一个定时器
    TimerID         m_id;
    //定时器的到期时间
    time_t          m_expiredTime;
    //定时器的定时时长(ms)
    int32_t         m_interval;
    //定时器重复触发的次数
    int32_t         m_repeatedTimes;
    //定时器触发后的回调函数
    TimerHandler   m_callback;
};

#endif
