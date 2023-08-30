#include "myAsyncHttpClient.h"

#include <winsock2.h>  //windows下调用网络编程的API所要引用的头文件
#include <WS2tcpip.h>

#include <sstream>
#include <thread>

#include "EventLoop.h"
#include "Platform.h"

std::mutex                      g_mutex;
std::condition_variable         g_cv;
std::mutex                      g_mutex1;
std::condition_variable         g_cv1;

enum class DecodeResult {
    Success,
    Failed,
    Partial
};

//封装了主类mySyncHttpClient所需的成员变量和成员函数
//在.cpp文件定义，对外不提供.cpp文件
struct AsyncImpl : public Connection
{
    bool                m_connected;
    bool                m_reservedHeaderInResponse;
    bool                m_busy;
    bool                m_keepAlive;

    EventLoop* m_eventLoop;
    int                 m_readWriteTimeoutMs;
    TimerID             m_timerID;

    bool                m_bInit;
    SOCKET              m_clientSocket;

    std::string         m_sendBuf;
    std::string         m_response;

    AsyncResultHandler  m_asyncResultHandler;

    std::unique_ptr<std::thread>    m_thread;

    //TODO: 初始化顺序需要根据字段定义顺序调整一下
    AsyncImpl(EventLoop* eventLoop) : m_connected(false), m_bInit(false),
        m_clientSocket(INVALID_SOCKET), m_eventLoop(eventLoop), m_timerID(TIMER_NOT_START),
        m_busy(false), m_keepAlive(true), m_reservedHeaderInResponse(true), m_readWriteTimeoutMs(3000), m_thread(nullptr) //初始化列表
    {

    }

    void threadProc()
    {
        m_eventLoop->setReadCallBack(std::bind(&AsyncImpl::onRead, this));
        m_eventLoop->setWriteCallBack(std::bind(&AsyncImpl::onWrite, this));
        m_eventLoop->runLoop();
    }

    void startLoop()
    {
        m_thread.reset(new std::thread(std::bind(&AsyncImpl::threadProc, this)));
    }

    void stopLoop()
    {
        m_eventLoop->Quit();
        m_thread->join();
    }

#ifdef WINDOWS
    bool InitSocket()
    {
        if (m_bInit)
            return true;

        //初始化socket库
        WORD wVersionRequested = MAKEWORD(2, 2);
        WSADATA wsaData;
        int err = ::WSAStartup(wVersionRequested, &wsaData);
        if (err != 0)
        {
            printf("WSAStartup failed with error: %d\n", err);
            return false;
        }
        if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
        {
            UnInitSocket();
            return false;
        }

        m_bInit = true;

        return true;
    }

    void UnInitSocket()
    {
        ::WSACleanup();
    }
#endif

    void closeDirectly()
    {//直接关闭socket
        if (m_connected) {
            m_eventLoop->removeFd(m_clientSocket);
            m_response.clear();
            ::closesocket(m_clientSocket);
            m_clientSocket = INVALID_SOCKET;
            m_connected = false;
        }
    }

    void doResultHandler(ResultCode resultCode) {
        if (resultCode == ResultCode::Success) {
            m_asyncResultHandler(resultCode, m_response);

            if (m_keepAlive) {
                closeDirectly();
            }

        }
        else {
            m_asyncResultHandler(resultCode, "");
            closeDirectly();
        }

        {
            //std::lock_guard<std::mutex> guard(g_mutex1);  //这里好像不需要用到信号量 ？
            m_busy = false;
        }
        //g_cv1.notify_one();

    }

    void onRead()
    {
        //读事件触发，开始收包
        bool success = recvData();

        if (!success) {
            //清除上一个定时器
            m_eventLoop->removeTimer(m_timerID);
            //收包出错
            doResultHandler(ResultCode::RecvDataError);
            return;
        }

        if (m_response.empty())
            return;

        //清除上一个定时器
        m_eventLoop->removeTimer(m_timerID);

        //TODO: 尝试解包，解包逻辑有待完善
        DecodeResult result = decodePackage();
        if (result == DecodeResult::Success) {
            doResultHandler(ResultCode::Success);
            return;
        }
        else if (result == DecodeResult::Failed) {
            doResultHandler(ResultCode::Failed);
            return;
        }

        //部分包
    }

    DecodeResult decodePackage() {
        //根据http报文格式
        //包已收全，根据m_reservedHeaderInResponse决定是否去掉reponse中的http头
        // 解包时，
        if (m_keepAlive) {
            //根据reponse中的connection字段设置m_keepAlive
            /*if (response中connection头的值是close)
                m_keepAlive = false;
            else
                m_keepAlive = true;*/
        }
        //非法包
        //包未收全
        return DecodeResult::Success;
    }

    void onWrite()
    {

        if (m_connected) {
            //清除上一个定时器
            m_eventLoop->removeTimer(m_timerID);


            //TCP窗口由不可写变为可写时触发的写事件
            // 
            //开启新的定时器，检测ConnectSocket可读或可写就绪是否超时，readWriteTimeoutHandler回调进行处理
            m_timerID = m_eventLoop->addTimer(m_readWriteTimeoutMs, 1, std::bind(&AsyncImpl::readWriteTimeoutHandler, this));
            //m_timerID = m_eventLoop->addTimer(m_readWriteTimeoutMs, 1, nullptr);

            //连接成功，发数据了
            bool success = sendData();
            if (!success) {
                doResultHandler(ResultCode::SendDataError);
                return;
            }

            if (m_sendBuf.empty()) {
                //全部数据都发出去了，注册读事件
                m_eventLoop->updateFd(m_clientSocket, READ_FLAG);
            }
            else {
                //部分数据发出去
                m_eventLoop->updateFd(m_clientSocket, WRITE_FLAG);
            }
        }
        else {
            //第一次连接结果
            onConnectResult();
        } //end if      
    }

    void onConnectResult() {
        //清除上一个定时器
        m_eventLoop->removeTimer(m_timerID);

#ifdef WINDOWS
        //连接成功了
        m_connected = true;
#else
        //判断socket是否有错误
        int optval;
        socklen_t optlen = static_cast<socklen_t>(sizeof optval);
        //linux使用getsockopt检测当前的socketfd是否有错，错误赋给optval，optval==0为不出错
        if (::getsockopt(m_clientSocket, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0 || optval != 0)
        {
            doResultHandler(ResultCode::ConnectError);
            return;
        }

        m_connected = true;
#endif
        //开启新的定时器，检测ConnectSocket可读或可写就绪是否超时，readWriteTimeoutHandler回调进行处理
        m_timerID = m_eventLoop->addTimer(m_readWriteTimeoutMs, 1, std::bind(&AsyncImpl::readWriteTimeoutHandler, this));

        //连接成功，发数据了
        bool success = sendData();
        if (!success) {
            doResultHandler(ResultCode::SendDataError);
            return;
        }

        if (m_sendBuf.empty()) {
            //全部数据都发出去了，注册读事件
            m_eventLoop->updateFd(m_clientSocket, READ_FLAG);
        }
        else {
            //部分数据发出去，要继续发
            m_eventLoop->updateFd(m_clientSocket, WRITE_FLAG);
        }
    }

    void readWriteTimeoutHandler() {
        //处理超时的情况
        //清除定时器
        m_eventLoop->removeTimer(m_timerID);

        //从eventLoop上移除fd
        m_eventLoop->removeFd(m_clientSocket);

        //关闭socket

        //通知用户收发数据超时了
        doResultHandler(ResultCode::ReadWriteTimeout);
    }

    void onError()
    {
        //TODO: 这里的处理逻辑是？
    }

    bool SetSocketNonBlocking(SOCKET& socket_fd)
    {
#ifdef WINDOWS
        unsigned long mode = 1;
        int result = ::ioctlsocket(socket_fd, FIONBIO, &mode);
        if (result != 0) //设置失败
            return false;
#else
        //获取socket状态标志
        int flags = fcntl(socket_fd, F_GETFL, 0); //F_GETFL: 获取文件状态标志
        if (flags == -1)    //获取失败
            return false;
        //将状态标志添加socket的 O_NONBLOCK
        int result = fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
        if (result == -1)   //设置失败
            return false;
#endif
        return true;
    }

    void connect(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port,
        bool reservedHeaderInResponse = false, int connectTimeoutMs = 3000, int readWriteTimeoutMs = 6000, bool keepAlive = true)
    {
        m_asyncResultHandler = asyncResultHandler;
        m_reservedHeaderInResponse = reservedHeaderInResponse;
        m_readWriteTimeoutMs = readWriteTimeoutMs;
        m_keepAlive = keepAlive;

        if (m_connected) {
            //直接发数据
            onWrite();
            return;
        }
#ifdef WINDOWS
        if (!InitSocket())
            return;
#endif

        //如果m_clientSocket正在尝试连接中，需把m_clientSocket挂载到I/O复用函数中，来检测m_clientSocket的读写就绪情况
        m_clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_clientSocket == INVALID_SOCKET)
        {
            doResultHandler(ResultCode::CreateSocketError);
            return;
        }

        //将socket设置成非阻塞的
        //fcntl/ioctlsocket
        if (!SetSocketNonBlocking(m_clientSocket))
        {
            closesocket(m_clientSocket);
            return;
        }
        struct sockaddr_in servaddr = { 0 };    //声明套接字目的地址结构
        //memset(&servaddr, 0, sizeof(servaddr));

        //检查host是否是域名，是的话需先将域名转为IP地址
        //inet_pton(AF_INET, host.c_str(), &servaddr.sin_addr);
        struct hostent* pHostent = NULL;
        if ((servaddr.sin_addr.s_addr = inet_addr(host.c_str())) == INADDR_NONE)
        {
            pHostent = ::gethostbyname(host.c_str());
            if (!pHostent)
            {
                printf("The Host Inputed Is Invalid.\n");
                printf("Please check your Network Connection.\n");
                doResultHandler(ResultCode::ConnectError);
                return;
            }
            else
                servaddr.sin_addr.s_addr = *((unsigned long*)pHostent->h_addr);
        }

        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons((u_short)SERV_PORT);
        ::connect(m_clientSocket, (sockaddr*)&servaddr, sizeof(servaddr));

        //挂载fd之前，要加个定时器，处理fd连接超时的情况
        m_timerID = m_eventLoop->addTimer(connectTimeoutMs, 1, std::bind(&AsyncImpl::connectTimeoutHandler, this));

        //将socket挂载到eventLoop上
        //连接成功或者失败情形：
        //在Windows上如果连接成功socket将变为可写，如果连接失败，socket不可写
        //在Linux上，如果连接成功，socket将可写，如果连接失败，socket将可读又可写
        m_eventLoop->addFd(m_clientSocket, WRITE_FLAG);

        //TODO: 将fd挂载上I/O复用检测后，需要有一个包装对象Connection来处理可读或可写事件被触发后的动作 （这里不知道怎么设计）

    }

    void connectTimeoutHandler() {
        m_eventLoop->removeTimer(m_timerID);

        //从eventLoop上移除fd
        m_eventLoop->removeFd(m_clientSocket);
        //关闭socket TODO

        //通知用户收发数据超时了
        doResultHandler(ResultCode::ConnectTimeout);
    }

    void makeRequest(const std::string& host, unsigned short port, const std::string& path,
        const std::string& method, const std::map<std::string, std::string>& customHeaders, const std::string& body, bool keepAlive)
    {
        //组装报文
        //请求报文的格式
        /*
            POST /path HTTP/1.1\r\n
            User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36\r\n
            Host: host:port\r\n
            \r\n
            httpbody
        */

        std::ostringstream sRequest;
        sRequest << method << " " << path << " HTTP/1.1\r\n";
        if (port != 80) //若不是80这个http的默认端口，要显示带上port
            sRequest << "Host: " << host << ":" << port << "\r\n";
        else
            sRequest << "Host: " << host << "\r\n";

        if (keepAlive) {
            sRequest << "Connection: " << "keep-alive" << "\r\n";
        }
        else {
            sRequest << "Connection: " << "close" << "\r\n";
        }

        //自定义的头部字段
        if (customHeaders.size() > 0)
        {
            for (const auto& iter : customHeaders)
            {
                sRequest << iter.first << "; " << iter.second << "\r\n";
            }
        }

        size_t bodySize = body.size();
        if (bodySize > 0)
            sRequest << "Content-Length: " << bodySize << "\r\n";

        //头部和body之间要有一个"\r\n"

        sRequest << "\r\n";

        if (bodySize > 0)
            sRequest << body;

        {
            std::lock_guard<std::mutex> guard(g_mutex);
            m_sendBuf = sRequest.str();
        }
        g_cv.notify_one();
    }

    bool sendData()
    {
        int n;
        std::unique_lock<std::mutex> guard(g_mutex);
        while (m_sendBuf.empty())
        {
            g_cv.wait(guard);
        }
        int bufferSize = m_sendBuf.size();
        while (true)
        {
            //MSG_NOSIGNAL是Linux下为了解决SIGPIPE引起程序崩溃问题，
            n = send(m_clientSocket, m_sendBuf.c_str(), m_sendBuf.size(), MSG_NOSIGNAL);
            if (n > 0)
            {
                // n > 0 是指已成功发送出去的数据的长度，因为有可能对端接收窗口大小有限，只能发送部分，剩下还在发送缓冲区里，等待下一次发送，直到发完
                m_sendBuf.erase(0, n);  //擦除掉已发送出去的部分
                if (m_sendBuf.empty())
                    break;
                else
                {
                    continue;
                }
            }
            else if (n == 0)
            {
                //对端关闭了连接
                return false;
            }
            else
            {
                // n < 0的情况
#ifdef WINDOWS
                int errorcode = ::WSAGetLastError();
                if (::WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    //由于TCP窗口太小，发不出，暂时退出
                    break;
                }

                return false;
#else
                if (errno == EINTR)
                {
                    continue;
                }
                else if (errno == EWOULDBLOCK || errno == EAGAIN)
                {
                    //由于TCP窗口太小，发不出，暂时退出
                    break;
                }

                return false;
#endif
            } // end if
        }// end while-loop

        return true;
    }

    bool recvData()
    {
        while (true)
        {
            char recvBuf[1024];
            int n = recv(m_clientSocket, recvBuf, 1024, 0);
            if (n == 0)
            {
                return false;
            }
            else if (n < 0)
            {
                int errocode = ::WSAGetLastError();
                if (errno == EWOULDBLOCK || ::WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    //if (m_response.empty())
                    //    continue;
                    //else
                    //    break;
                    break;
                }
                if (errno == ECONNRESET || ::WSAGetLastError() == WSAECONNRESET)
                {
                    //return false;
                    break;
                }
            }
            else
            {
                //大于0
                m_response.append(recvBuf, n);
            }
        }// end while-loop

        return true;
    }//end recvData


};


myAsyncHttpClient::myAsyncHttpClient(EventLoop* eventLoop)
{
    m_impl = std::make_unique<AsyncImpl>(eventLoop);  //智能指针初始化
}

myAsyncHttpClient::~myAsyncHttpClient()
{
#ifdef WINDOWS
    if (m_impl->m_bInit)
        m_impl->UnInitSocket();
#endif
}

void myAsyncHttpClient::startWork()
{
    m_impl->startLoop();
}

void myAsyncHttpClient::endWork()
{
    m_impl->stopLoop();
}

bool myAsyncHttpClient::getIsBusy()
{
    return m_impl->m_busy;
}

bool myAsyncHttpClient::myGet(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path, const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response, bool reservedHeaderInResponse, CompressMethod compressMethod, int connectTimeout, int readTimeout, int writeTimeout)
{
    //暂不实现
    return false;
}

bool myAsyncHttpClient::myPost(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path, const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response, bool reservedHeaderInResponse, CompressMethod compressMethod, int connectTimeout, int readTimeout, int writeTimeout)
{
    //暂不实现
    return false;
}

void myAsyncHttpClient::myRequest(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path,
    const std::string& method,
    const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response,
    bool reservedHeaderInResponse /*= false*/, CompressMethod compressMethod /*= CompressMethod::none*/,
    int connectTimeoutMs/* = 3000*/, int readWriteTimeoutMs/* = 6000*/, bool keepAlive /*= true*/)
{
    if (m_impl->m_busy) {
        return;
    }

    m_impl->m_busy = true;

    m_impl->makeRequest(host, port, path, method, customHeaders, body, keepAlive);

    m_impl->connect(std::move(asyncResultHandler), host, port, reservedHeaderInResponse, connectTimeoutMs, readWriteTimeoutMs);
}
