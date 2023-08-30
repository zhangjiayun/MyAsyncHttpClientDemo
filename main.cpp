#include <iostream>
#include "myAsyncHttpClient.h"
#include "EventLoop.h"
#include <string>
#include <algorithm>

/*
* һ�����첽httpclientDemo��ͨ��������ַ��path������ʽ������ȡ������Response
*/

void myHttpResultHandler(ResultCode resultCode, const std::string& response)
{
    switch (resultCode)
    {
    case ResultCode::Success:
    {
        std::cout << "Response Successfully, the content is:\n" << response << std::endl;
    }
    break;

    case ResultCode::ConnectTimeout:
    {
        std::cout << "ConnectTimeout" << std::endl;
    }
    break;

    case ResultCode::CreateSocketError:
    {
        std::cout << "CreateSocketError" << std::endl;
    }
    break;

    case ResultCode::SendDataError:
    {
        std::cout << "SendDataError" << std::endl;
    }
    break;

    case ResultCode::RecvDataError:
    {
        std::cout << "RecvDataError" << std::endl;
    }
    break;

    case ResultCode::ReadWriteTimeout:
    {
        std::cout << "ReadWriteTimeout" << std::endl;
    }
    break;

    case ResultCode::ConnectError:
    {
        std::cout << "ConnectError" << std::endl;
    }
    break;

    default:
        break;
    }
}

int main()
{
    EventLoop* peventLoop = new EventLoop();
    myAsyncHttpClient myHttpClient(peventLoop);
    myHttpClient.startWork();
    std::string sFlag;
    std::string sHost;
    std::string sPath;
    std::string sMethod;
    std::map<std::string, std::string> mCustomHeaders;
    mCustomHeaders.clear();
    std::string sBpdy = "";
    std::string sResponse;
    std::cout << "Start my Client now? (Yes or No)" << std::endl;
    getline(std::cin, sFlag);
    transform(sFlag.begin(), sFlag.end(), sFlag.begin(), ::toupper);    //ת�ɴ�д
    if (sFlag == "YES")
    {
        while (true)
        {
            {
                //std::unique_lock<std::mutex> guard(g_mutex1);         //�� �������ﲻ��Ҫ�õ��ź���
                //while (myHttpClient.getIsBusy() == true)
                //{
                //    g_cv1.wait(guard);
                //}
                if (myHttpClient.getIsBusy())
                    continue;
            }
            std::cout << std::endl;
            std::cout << "Continue or not? (Yes or No)" << std::endl;
            getline(std::cin, sFlag);
            transform(sFlag.begin(), sFlag.end(), sFlag.begin(), ::toupper);    //ת�ɴ�д
            if (sFlag == "NO")
            {
                myHttpClient.endWork();
                break;
            }
            //������Request�ر�����Ϣ
            std::cout << "Please input the host:" << std::endl;
            getline(std::cin, sHost);
            std::cout << "Please input the path:" << std::endl;
            getline(std::cin, sPath);
            std::cout << "Please input the request method:" << std::endl;
            getline(std::cin, sMethod);
            transform(sMethod.begin(), sMethod.end(), sMethod.begin(), ::toupper);    //ת�ɴ�д

            myHttpClient.myRequest(myHttpResultHandler, sHost, 80, sPath, sMethod, mCustomHeaders, sBpdy, sResponse);

        }
    }

    return 0;
}
