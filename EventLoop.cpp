
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
        //1. ��ʱ�����Ͷ�ʱ���¼�����
        checkAndHandleTimers();

        //2. ����IO���ú������󶨵�IO���ú����ϵ�socket�Ķ�д�¼�
        int nready = selectHandle();
        if (nready >= 1)
        {
            auto maxFD = m_FDs.rbegin();
            //������Ʋ����ƣ�ֻ�ܴ���һ��connectfd

            //3. �������¼���socket�ϵĶ�д�¼�
            if (FD_ISSET(*maxFD, &m_allWriteSet))
            {
                if (m_writeCallBack)
                    m_writeCallBack();
            }

            if (FD_ISSET(*maxFD, &m_allReadSet))
            {
                if (m_readCallBack)
                    m_readCallBack();
                int m = 0;
            }
        }

        //4. ��һЩ��������
    }
}

TimerID EventLoop::addTimer(int64_t intervalMs, int32_t repeatCount, const TimerHandler& timerCallback)
{
    Timer* pTimer = new Timer(intervalMs, repeatCount, timerCallback);

    TimerID timerId = pTimer->getId();

    //С����
    timers_queue_.push(pTimer);
    timer_mp_[timerId] = pTimer;
    return timerId;
}

bool EventLoop::removeTimer(TimerID timerID)
{
    auto iter = timer_mp_.find(timerID);
    if (iter != timer_mp_.end())
    {
        iter->second->setExpiredTime(-1);   //����Ҫ��ɾ����Ԫ�ؾͻ��ϳ����Ѷ�
        timers_queue_.pop();
        timer_mp_.erase(iter);
        return true;
    }
    return false;
}

void EventLoop::checkAndHandleTimers()
{
    //С����
    int len = timers_queue_.size();
    for (int i = 0; i < len; i++)
    {
        Timer* delete_timer;
        delete_timer = timers_queue_.top();
        if (delete_timer->isExpired())
        {
            delete_timer->run();
            if (delete_timer->getRepeatedTimes() == 0)  //��ʱ��������Ч�ˣ�ɾ��
            {
                timer_mp_.erase(delete_timer->getId());
                delete delete_timer;
                timers_queue_.pop();
            }
        }
        else
            return;     //����Ķ�ʱ��������Ҫ����ˣ���Ϊ���ĳ�ʱʱ��϶��ȵ�ǰ��ʱ���ĳ�ʱʱ����
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
    FD_CLR(fd, &m_allWriteSet);
    FD_CLR(fd, &m_allReadSet);
    if (m_FDs.count(fd) > 0)
        m_FDs.erase(fd);
}

void EventLoop::updateFd(SOCKET fd, int8_t eventFlags)
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

int EventLoop::selectHandle()
{
    if (m_FDs.empty())
        return -1;

    auto maxFD = m_FDs.rbegin();
    fd_set rset = m_allReadSet;
    fd_set wset = m_allWriteSet;

    int nready = ::select(*maxFD + 1, &rset, &wset, NULL, NULL);
    return nready;
}
