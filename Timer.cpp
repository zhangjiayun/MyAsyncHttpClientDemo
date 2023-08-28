#include "Timer.h"

std::atomic<TimerID> Timer::s_initialId = 0;

Timer::Timer(int64_t intervalMs, int32_t repeatCount, const TimerHandler& timerHandler)
    : m_interval(intervalMs), m_repeatedTimes(repeatCount), m_callback(timerHandler)
{
    //��ǰʱ����ϴ�������õ���һ�εĹ���ʱ��
    m_expiredTime = (int64_t)time(nullptr) + intervalMs;
    m_id = Timer::generateId();
}

void Timer::run()
{
    m_callback();

    if (m_repeatedTimes >= 1)
    {
        --m_repeatedTimes;
    }

    //������һ�εĴ���ʱ��
    m_expiredTime += m_interval;
}

bool Timer::isExpired()
{
    int64_t now = time(nullptr);
    return now >= m_expiredTime;
}

TimerID Timer::generateId()
{
    TimerID tmpId;
    s_initialId++;
    tmpId = s_initialId;
    return tmpId;
}
