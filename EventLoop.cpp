
#include "EventLoop.h"

EventLoop::EventLoop()
{
    timer_mp_.clear();
    FD_ZERO(&m_allReadSet);
    FD_ZERO(&m_allWriteSet);
    m_FDs.clear();
    m_bQuitFlag = false;

    m_readCallBack = NULL;
    m_writeCallBack = NULL;
}

void EventLoop::runLoop()
{
    while (!m_bQuitFlag)
    {
        //1. 定时器检测和定时器事件处理
        checkAndHandleTimers();

        //2. 利用IO复用函数检测绑定到IO复用函数上的socket的读写事件
        int nready = selectHandle();
        if (nready >= 1)
        {
            auto maxFD = m_FDs.rbegin();
            //这里设计不完善，只能处理一个connectfd

            //3. 处理有事件的socket上的读写事件
            if (FD_ISSET(*maxFD, &m_allWriteSet))
            {
                if (m_writeCallBack)
                    m_writeCallBack();
            }

            if (FD_ISSET(*maxFD, &m_allReadSet))
            {
                if (m_readCallBack)
                    m_readCallBack();
            }
        }

        //4. 做一些其他事情
        std::this_thread::sleep_for(std::chrono::microseconds(200));    //在这里设置200ms的休眠，否则CPU占用会有点高（权宜之计）
    }
}

TimerID EventLoop::addTimer(int64_t intervalMs, int32_t repeatCount, const TimerHandler& timerCallback)
{
    Timer* pTimer = new Timer(intervalMs, repeatCount, timerCallback);

    TimerID timerId = pTimer->getId();

    //小根堆
    timers_queue_.push(pTimer);
    timer_mp_[timerId] = pTimer;
    return timerId;
}

bool EventLoop::removeTimer(TimerID timerID)
{
    auto iter = timer_mp_.find(timerID);
    if (iter != timer_mp_.end())
    {
        iter->second->setExpiredTime(-1);   //这样要被删除的元素就会上沉到堆顶
        timers_queue_.pop();
        timer_mp_.erase(iter);
        return true;
    }
    return false;
}

void EventLoop::checkAndHandleTimers()
{
    //小根堆
    int len = timers_queue_.size();
    for (int i = 0; i < len; i++)
    {
        Timer* delete_timer;
        delete_timer = timers_queue_.top();
        if (delete_timer->isExpired())
        {
            delete_timer->run();
            if (delete_timer->getRepeatedTimes() == 0)  //定时器对象无效了，删除
            {
                removeTimer(delete_timer->getId());
            }
        }
        else
            return;     //后面的定时器都不需要检测了，因为它的超时时间肯定比当前定时器的超时时间晚
    }
}

void EventLoop::addFd(SOCKET fd, int8_t eventFlags)
{
    switch (eventFlags)
    {
    case WRITE_FLAG: FD_SET(fd, &m_allWriteSet); break;

    case READ_FLAG: FD_SET(fd, &m_allReadSet); break;

    default: return;
        break;
    }

    if (m_FDs.count(fd) == 0)
        m_FDs.insert(fd);
}

void EventLoop::removeFd(SOCKET fd)
{
    if (m_FDs.count(fd) > 0)
    {
        m_FDs.erase(fd);
        FD_CLR(fd, &m_allWriteSet);
        FD_CLR(fd, &m_allReadSet);
    }
}

void EventLoop::updateFd(SOCKET fd, int8_t eventFlags)
{
    switch (eventFlags)
    {
    case WRITE_FLAG:
    {
        FD_SET(fd, &m_allWriteSet);
        FD_CLR(fd, &m_allReadSet);
    }
    break;

    case READ_FLAG:
    {
        FD_SET(fd, &m_allReadSet);
        FD_CLR(fd, &m_allWriteSet);
    }
    break;

    default: return;
        break;
    }

    if (m_FDs.count(fd) == 0)
        m_FDs.insert(fd);
}

int EventLoop::selectHandle()
{
    if (m_FDs.empty())
        return -1;

    auto maxFD = m_FDs.rbegin();

    int nready = ::select(*maxFD + 1, &m_allReadSet, &m_allWriteSet, NULL, NULL);
    return nready;
}
