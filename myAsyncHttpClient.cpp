#include "myAsyncHttpClient.h"

#include <winsock2.h>  //windows�µ��������̵�API��Ҫ���õ�ͷ�ļ�
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

//��װ������mySyncHttpClient����ĳ�Ա�����ͳ�Ա����
//��.cpp�ļ����壬���ⲻ�ṩ.cpp�ļ�
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

    //TODO: ��ʼ��˳����Ҫ�����ֶζ���˳�����һ��
    AsyncImpl(EventLoop* eventLoop) : m_connected(false), m_bInit(false),
        m_clientSocket(INVALID_SOCKET), m_eventLoop(eventLoop), m_timerID(TIMER_NOT_START),
        m_busy(false), m_keepAlive(true), m_reservedHeaderInResponse(true), m_readWriteTimeoutMs(3000), m_thread(nullptr) //��ʼ���б�
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

        //��ʼ��socket��
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
    {//ֱ�ӹر�socket
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
            //std::lock_guard<std::mutex> guard(g_mutex1);  //���������Ҫ�õ��ź��� ��
            m_busy = false;
        }
        //g_cv1.notify_one();

    }

    void onRead()
    {
        //���¼���������ʼ�հ�
        bool success = recvData();

        if (!success) {
            //�����һ����ʱ��
            m_eventLoop->removeTimer(m_timerID);
            //�հ�����
            doResultHandler(ResultCode::RecvDataError);
            return;
        }

        if (m_response.empty())
            return;

        //�����һ����ʱ��
        m_eventLoop->removeTimer(m_timerID);

        //TODO: ���Խ��������߼��д�����
        DecodeResult result = decodePackage();
        if (result == DecodeResult::Success) {
            doResultHandler(ResultCode::Success);
            return;
        }
        else if (result == DecodeResult::Failed) {
            doResultHandler(ResultCode::Failed);
            return;
        }

        //���ְ�
    }

    DecodeResult decodePackage() {
        //����http���ĸ�ʽ
        //������ȫ������m_reservedHeaderInResponse�����Ƿ�ȥ��reponse�е�httpͷ
        // ���ʱ��
        if (m_keepAlive) {
            //����reponse�е�connection�ֶ�����m_keepAlive
            /*if (response��connectionͷ��ֵ��close)
                m_keepAlive = false;
            else
                m_keepAlive = true;*/
        }
        //�Ƿ���
        //��δ��ȫ
        return DecodeResult::Success;
    }

    void onWrite()
    {

        if (m_connected) {
            //�����һ����ʱ��
            m_eventLoop->removeTimer(m_timerID);


            //TCP�����ɲ���д��Ϊ��дʱ������д�¼�
            // 
            //�����µĶ�ʱ�������ConnectSocket�ɶ����д�����Ƿ�ʱ��readWriteTimeoutHandler�ص����д���
            m_timerID = m_eventLoop->addTimer(m_readWriteTimeoutMs, 1, std::bind(&AsyncImpl::readWriteTimeoutHandler, this));
            //m_timerID = m_eventLoop->addTimer(m_readWriteTimeoutMs, 1, nullptr);

            //���ӳɹ�����������
            bool success = sendData();
            if (!success) {
                doResultHandler(ResultCode::SendDataError);
                return;
            }

            if (m_sendBuf.empty()) {
                //ȫ�����ݶ�����ȥ�ˣ�ע����¼�
                m_eventLoop->updateFd(m_clientSocket, READ_FLAG);
            }
            else {
                //�������ݷ���ȥ
                m_eventLoop->updateFd(m_clientSocket, WRITE_FLAG);
            }
        }
        else {
            //��һ�����ӽ��
            onConnectResult();
        } //end if      
    }

    void onConnectResult() {
        //�����һ����ʱ��
        m_eventLoop->removeTimer(m_timerID);

#ifdef WINDOWS
        //���ӳɹ���
        m_connected = true;
#else
        //�ж�socket�Ƿ��д���
        int optval;
        socklen_t optlen = static_cast<socklen_t>(sizeof optval);
        //linuxʹ��getsockopt��⵱ǰ��socketfd�Ƿ��д����󸳸�optval��optval==0Ϊ������
        if (::getsockopt(m_clientSocket, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0 || optval != 0)
        {
            doResultHandler(ResultCode::ConnectError);
            return;
        }

        m_connected = true;
#endif
        //�����µĶ�ʱ�������ConnectSocket�ɶ����д�����Ƿ�ʱ��readWriteTimeoutHandler�ص����д���
        m_timerID = m_eventLoop->addTimer(m_readWriteTimeoutMs, 1, std::bind(&AsyncImpl::readWriteTimeoutHandler, this));

        //���ӳɹ�����������
        bool success = sendData();
        if (!success) {
            doResultHandler(ResultCode::SendDataError);
            return;
        }

        if (m_sendBuf.empty()) {
            //ȫ�����ݶ�����ȥ�ˣ�ע����¼�
            m_eventLoop->updateFd(m_clientSocket, READ_FLAG);
        }
        else {
            //�������ݷ���ȥ��Ҫ������
            m_eventLoop->updateFd(m_clientSocket, WRITE_FLAG);
        }
    }

    void readWriteTimeoutHandler() {
        //����ʱ�����
        //�����ʱ��
        m_eventLoop->removeTimer(m_timerID);

        //��eventLoop���Ƴ�fd
        m_eventLoop->removeFd(m_clientSocket);

        //�ر�socket

        //֪ͨ�û��շ����ݳ�ʱ��
        doResultHandler(ResultCode::ReadWriteTimeout);
    }

    void onError()
    {
        //TODO: ����Ĵ����߼��ǣ�
    }

    bool SetSocketNonBlocking(SOCKET& socket_fd)
    {
#ifdef WINDOWS
        unsigned long mode = 1;
        int result = ::ioctlsocket(socket_fd, FIONBIO, &mode);
        if (result != 0) //����ʧ��
            return false;
#else
        //��ȡsocket״̬��־
        int flags = fcntl(socket_fd, F_GETFL, 0); //F_GETFL: ��ȡ�ļ�״̬��־
        if (flags == -1)    //��ȡʧ��
            return false;
        //��״̬��־���socket�� O_NONBLOCK
        int result = fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
        if (result == -1)   //����ʧ��
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
            //ֱ�ӷ�����
            onWrite();
            return;
        }
#ifdef WINDOWS
        if (!InitSocket())
            return;
#endif

        //���m_clientSocket���ڳ��������У����m_clientSocket���ص�I/O���ú����У������m_clientSocket�Ķ�д�������
        m_clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_clientSocket == INVALID_SOCKET)
        {
            doResultHandler(ResultCode::CreateSocketError);
            return;
        }

        //��socket���óɷ�������
        //fcntl/ioctlsocket
        if (!SetSocketNonBlocking(m_clientSocket))
        {
            closesocket(m_clientSocket);
            return;
        }
        struct sockaddr_in servaddr = { 0 };    //�����׽���Ŀ�ĵ�ַ�ṹ
        //memset(&servaddr, 0, sizeof(servaddr));

        //���host�Ƿ����������ǵĻ����Ƚ�����תΪIP��ַ
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

        //����fd֮ǰ��Ҫ�Ӹ���ʱ��������fd���ӳ�ʱ�����
        m_timerID = m_eventLoop->addTimer(connectTimeoutMs, 1, std::bind(&AsyncImpl::connectTimeoutHandler, this));

        //��socket���ص�eventLoop��
        //���ӳɹ�����ʧ�����Σ�
        //��Windows��������ӳɹ�socket����Ϊ��д���������ʧ�ܣ�socket����д
        //��Linux�ϣ�������ӳɹ���socket����д���������ʧ�ܣ�socket���ɶ��ֿ�д
        m_eventLoop->addFd(m_clientSocket, WRITE_FLAG);

        //TODO: ��fd������I/O���ü�����Ҫ��һ����װ����Connection������ɶ����д�¼���������Ķ��� �����ﲻ֪����ô��ƣ�

    }

    void connectTimeoutHandler() {
        m_eventLoop->removeTimer(m_timerID);

        //��eventLoop���Ƴ�fd
        m_eventLoop->removeFd(m_clientSocket);
        //�ر�socket TODO

        //֪ͨ�û��շ����ݳ�ʱ��
        doResultHandler(ResultCode::ConnectTimeout);
    }

    void makeRequest(const std::string& host, unsigned short port, const std::string& path,
        const std::string& method, const std::map<std::string, std::string>& customHeaders, const std::string& body, bool keepAlive)
    {
        //��װ����
        //�����ĵĸ�ʽ
        /*
            POST /path HTTP/1.1\r\n
            User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36\r\n
            Host: host:port\r\n
            \r\n
            httpbody
        */

        std::ostringstream sRequest;
        sRequest << method << " " << path << " HTTP/1.1\r\n";
        if (port != 80) //������80���http��Ĭ�϶˿ڣ�Ҫ��ʾ����port
            sRequest << "Host: " << host << ":" << port << "\r\n";
        else
            sRequest << "Host: " << host << "\r\n";

        if (keepAlive) {
            sRequest << "Connection: " << "keep-alive" << "\r\n";
        }
        else {
            sRequest << "Connection: " << "close" << "\r\n";
        }

        //�Զ����ͷ���ֶ�
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

        //ͷ����body֮��Ҫ��һ��"\r\n"

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
            //MSG_NOSIGNAL��Linux��Ϊ�˽��SIGPIPE�������������⣬
            n = send(m_clientSocket, m_sendBuf.c_str(), m_sendBuf.size(), MSG_NOSIGNAL);
            if (n > 0)
            {
                // n > 0 ��ָ�ѳɹ����ͳ�ȥ�����ݵĳ��ȣ���Ϊ�п��ܶԶ˽��մ��ڴ�С���ޣ�ֻ�ܷ��Ͳ��֣�ʣ�»��ڷ��ͻ�������ȴ���һ�η��ͣ�ֱ������
                m_sendBuf.erase(0, n);  //�������ѷ��ͳ�ȥ�Ĳ���
                if (m_sendBuf.empty())
                    break;
                else
                {
                    continue;
                }
            }
            else if (n == 0)
            {
                //�Զ˹ر�������
                return false;
            }
            else
            {
                // n < 0�����
#ifdef WINDOWS
                int errorcode = ::WSAGetLastError();
                if (::WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    //����TCP����̫С������������ʱ�˳�
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
                    //����TCP����̫С������������ʱ�˳�
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
                //����0
                m_response.append(recvBuf, n);
            }
        }// end while-loop

        return true;
    }//end recvData


};


myAsyncHttpClient::myAsyncHttpClient(EventLoop* eventLoop)
{
    m_impl = std::make_unique<AsyncImpl>(eventLoop);  //����ָ���ʼ��
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
    //�ݲ�ʵ��
    return false;
}

bool myAsyncHttpClient::myPost(AsyncResultHandler&& asyncResultHandler, const std::string& host, unsigned short port, const std::string& path, const std::map<std::string, std::string>& customHeaders, const std::string& body, std::string& response, bool reservedHeaderInResponse, CompressMethod compressMethod, int connectTimeout, int readTimeout, int writeTimeout)
{
    //�ݲ�ʵ��
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
