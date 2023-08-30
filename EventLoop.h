#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <stdint.h>

#include <functional>
#include <winsock2.h>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include "Platform.h"
#include "Timer.h"

#define READ_FLAG   0x0001
#define WRITE_FLAG  0x0010

#define TIMER_NOT_START -1

typedef std::function<void()> ReadCallBack;
typedef std::function<void()> WriteCallBack;

struct TimerCompare
{
    bool operator () (const Timer* lhs, const Timer* rhs) const //�˴�Ҫ��const
    {
        //return lhs->getExpiredTime() < rhs->getExpiredTime();
        return lhs->getExpiredTime() > rhs->getExpiredTime();
    }
};

//��������д����������������Ӵ��󣬲�����
//auto cmp = [](Timer* t1, Timer* t2) {return t1->getExpiredTime() > t2->getExpiredTime(); };//�Զ������ӣ�ʹ��priority_queue�е�Խ���ʱ�Ķ�ʱ������ǰ��

struct AsyncImpl;
//����I/O���ü��
class EventLoop
{
public:
    EventLoop();

    void runLoop();

public:
    //��Ӷ�ʱ��
    //timerHandlerΪ��ʱ���ص�������
    //TimerID addTimer(int64_t intervalMs, int32_t repeactCount, TimerHandler&& timerHandler);      
    TimerID addTimer(int64_t intervalMs, int32_t repeatCount, const TimerHandler& timerCallback);

    bool removeTimer(TimerID timerID);
    void checkAndHandleTimers();
private:
    //ʹ��std::priority_queue (С����)������
    std::priority_queue<Timer*, std::vector<Timer*>, TimerCompare> timers_queue_;//�������ж�ʱ�������ݽṹ
    std::unordered_map<int64_t, Timer*> timer_mp_;  //priority_queueֻ�ܷ��ʶѶ�Ԫ�أ�����Ҫ����һ��map����¼һ�£������ֶ�ɾ����ʱ��


public:
    void addFd(SOCKET fd, int8_t eventFlags);  //��socketID�󶨵�I/O���ú���
    void removeFd(SOCKET fd);
    void updateFd(SOCKET fd, int8_t eventFlags);
    int selectHandle();
private:
    fd_set m_allReadSet;
    fd_set m_allWriteSet;
    std::set<SOCKET> m_FDs; //������������socket fd

private:
    bool m_bQuitFlag;
public:
    void Quit() { m_bQuitFlag = true; };
    bool getQuitFlag() { return m_bQuitFlag; };

private:
    ReadCallBack    m_readCallBack;
    WriteCallBack   m_writeCallBack;
public:
    void setReadCallBack(const ReadCallBack& readCallback) { m_readCallBack = readCallback; };
    void setWriteCallBack(const WriteCallBack& writeCallback) { m_writeCallBack = writeCallback; };
};

class Connection {                  //����ӿ���û���ϣ���֪����ô��ƣ�����
    virtual void onRead() = 0;
    virtual void onWrite() = 0;
    virtual void onError() = 0;
};

#endif //!EVENT_LOOP_H
