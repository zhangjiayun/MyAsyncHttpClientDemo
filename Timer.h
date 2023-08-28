#ifndef _TIMER_H
#define _TIMER_H

#include <atomic>
#include <functional>
#include <stdint.h>
typedef std::function<void()> TimerHandler;

typedef int64_t TimerID;  //int64Ҫ����stdint.h

//using TimerHandler = std::function<void(void* param)>;

class Timer
{
public:
    Timer(int64_t intervalMs, int32_t repeatCount, const TimerHandler& timerCallback);
    ~Timer() {};

    void run();

    bool isExpired();      //�ж϶�ʱ���Ƿ���
    TimerID getId() { return m_id; };
    int32_t getRepeatedTimes() { return m_repeatedTimes; };
    time_t getExpiredTime() const { return m_expiredTime; };
    void setExpiredTime(time_t itime) { m_expiredTime = itime; };

public:
    //����һ��Ψһ��id
    static TimerID generateId();

private:
    //��ʱ��id��׼ֵ����ʼֵΪ0
    static std::atomic<TimerID> s_initialId;

private:
    //��ʱ����id��Ψһ��ʶһ����ʱ��
    TimerID         m_id;
    //��ʱ���ĵ���ʱ��
    time_t          m_expiredTime;
    //��ʱ���Ķ�ʱʱ��(ms)
    int32_t         m_interval;
    //��ʱ���ظ������Ĵ���
    int32_t         m_repeatedTimes;
    //��ʱ��������Ļص�����
    TimerHandler   m_callback;
};

#endif
