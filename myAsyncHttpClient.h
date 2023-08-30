#ifndef MYASYNC_HTTP_CLIENT_H
#define MYASYNC_HTTP_CLIENT_H

#include <functional>
#include <map>
#include <memory>  //使用智能指针
#include <string>

#include <mutex>
#include <condition_variable>

#include "Platform.h"

//pimpl惯用法   //将主类A中用到的成员变量和成员函数封装在一个独立类或者结构B，然后在主类A中声明B的成员变量指针来使用
//用途:可将B的声明放在.h文件中对外提供，实现则放在.cpp中不对外，可增加保密性

struct AsyncImpl;

class EventLoop;

enum class CompressMethod  //http报文的压缩方式
{
    none,
    gzip,
    deflate,
    br
};

//C++ 11新枚举定义 enum class
//枚举结果原因码
enum class ResultCode
{
    Success,
    CreateSocketError,
    ConnectError,
    SendDataError,
    RecvDataError,
    Failed,
    ConnectTimeout,
    ReadWriteTimeout
};

//定义一个异步结果处理回调函数
//由调用者传回调函数地址进来。等有结果时，通过回调告诉调用者
//void AsyncResultHandler(int resultCode, const std::string& response);
//C++ 11语法将其包装成一个函数指针
using AsyncResultHandler = std::function<void(ResultCode resultCode, const std::string& response)>;

class myAsyncHttpClient
{
public:
    myAsyncHttpClient(EventLoop* eventLoop);
    ~myAsyncHttpClient();

public:
    void startWork();
    void endWork();
    void startRequest();
    bool getIsBusy();

public:
    //为了复用，定义成右值AsyncResultHandler&& asyncResultHandler
    void myRequest(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path,
        const std::string& method,
        const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response,
        bool reservedHeaderInResponse = false, CompressMethod compressMethod = CompressMethod::none,
        int connectTimeoutMs = 3, int readWriteTimeoutMs = 4, bool keepAlive = true); //


    bool myGet(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path,
        const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response,
        bool reservedHeaderInResponse = false, CompressMethod compressMethod = CompressMethod::none, int connectTimeout = 3, int readTimeout = 10, int writeTimeout = 10);


    // Http请求中的Post方式
    // 为啥要设计成 AsyncResultHandler&& ？
    bool myPost(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path,
        const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response,
        bool reservedHeaderInResponse = false, CompressMethod compressMethod = CompressMethod::none, int connectTimeout = 3, int readTimeout = 10, int writeTimeout = 10);

private:
    std::unique_ptr<AsyncImpl>      m_impl;   //智能指针，自动释放
};

extern std::mutex                      g_mutex;
extern std::condition_variable         g_cv;
extern std::mutex                      g_mutex1;
extern std::condition_variable         g_cv1;
#endif //!MYASYNC_HTTP_CLIENT_H
