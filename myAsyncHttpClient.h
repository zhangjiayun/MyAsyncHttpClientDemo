#ifndef MYASYNC_HTTP_CLIENT_H
#define MYASYNC_HTTP_CLIENT_H

#include <functional>
#include <map>
#include <memory>  //ʹ������ָ��
#include <string>

#include <mutex>
#include <condition_variable>

#include "Platform.h"

//pimpl���÷�   //������A���õ��ĳ�Ա�����ͳ�Ա������װ��һ����������߽ṹB��Ȼ��������A������B�ĳ�Ա����ָ����ʹ��
//��;:�ɽ�B����������.h�ļ��ж����ṩ��ʵ�������.cpp�в����⣬�����ӱ�����

struct AsyncImpl;

class EventLoop;

enum class CompressMethod  //http���ĵ�ѹ����ʽ
{
    none,
    gzip,
    deflate,
    br
};

//C++ 11��ö�ٶ��� enum class
//ö�ٽ��ԭ����
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

//����һ���첽�������ص�����
//�ɵ����ߴ��ص�������ַ���������н��ʱ��ͨ���ص����ߵ�����
//void AsyncResultHandler(int resultCode, const std::string& response);
//C++ 11�﷨�����װ��һ������ָ��
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
    //Ϊ�˸��ã��������ֵAsyncResultHandler&& asyncResultHandler
    void myRequest(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path,
        const std::string& method,
        const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response,
        bool reservedHeaderInResponse = false, CompressMethod compressMethod = CompressMethod::none,
        int connectTimeoutMs = 3, int readWriteTimeoutMs = 4, bool keepAlive = true); //


    bool myGet(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path,
        const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response,
        bool reservedHeaderInResponse = false, CompressMethod compressMethod = CompressMethod::none, int connectTimeout = 3, int readTimeout = 10, int writeTimeout = 10);


    // Http�����е�Post��ʽ
    // ΪɶҪ��Ƴ� AsyncResultHandler&& ��
    bool myPost(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path,
        const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response,
        bool reservedHeaderInResponse = false, CompressMethod compressMethod = CompressMethod::none, int connectTimeout = 3, int readTimeout = 10, int writeTimeout = 10);

private:
    std::unique_ptr<AsyncImpl>      m_impl;   //����ָ�룬�Զ��ͷ�
};

extern std::mutex                      g_mutex;
extern std::condition_variable         g_cv;
extern std::mutex                      g_mutex1;
extern std::condition_variable         g_cv1;
#endif //!MYASYNC_HTTP_CLIENT_H
