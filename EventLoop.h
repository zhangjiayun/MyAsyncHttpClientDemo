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
    bool operator () (const Timer* lhs, const Timer* rhs) const //此处要加const
    {
        //return lhs->getExpiredTime() < rhs->getExpiredTime();
        return lhs->getExpiredTime() > rhs->getExpiredTime();
    }
};

//想用这种写法，但编译出现链接错误，不明白
//auto cmp = [](Timer* t1, Timer* t2) {return t1->getExpiredTime() > t2->getExpiredTime(); };//自定义算子，使得priority_queue中的越早过时的定时器排在前面

struct AsyncImpl;
//用于I/O复用检测
class EventLoop
{
public:
    EventLoop();

    void runLoop();

public:
    //添加定时器
    //timerHandler为定时器回调处理函数
    //TimerID addTimer(int64_t intervalMs, int32_t repeactCount, TimerHandler&& timerHandler);      
    TimerID addTimer(int64_t intervalMs, int32_t repeatCount, const TimerHandler& timerCallback);

    bool removeTimer(TimerID timerID);
    void checkAndHandleTimers();
private:
    //使用std::priority_queue (小根堆)来管理
    std::priority_queue<Timer*, std::vector<Timer*>, TimerCompare> timers_queue_;//管理所有定时器的数据结构
    std::unordered_map<int64_t, Timer*> timer_mp_;  //priority_queue只能访问堆顶元素，所以要用了一个map来记录一下，方便手动删除定时器


public:
    void addFd(SOCKET fd, int8_t eventFlags);  //将socketID绑定到I/O复用函数
    void removeFd(SOCKET fd);
    void updateFd(SOCKET fd, int8_t eventFlags);
    int selectHandle();
private:
    fd_set m_allReadSet;
    fd_set m_allWriteSet;
    std::set<SOCKET> m_FDs; //用来管理所有socket fd

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

class Connection {                  //这个接口类没用上，不知道怎么设计？保留
    virtual void onRead() = 0;
    virtual void onWrite() = 0;
    virtual void onError() = 0;
};

#endif //!EVENT_LOOP_H
