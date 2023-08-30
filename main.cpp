#include <iostream>
#include "myAsyncHttpClient.h"
#include "EventLoop.h"
#include <string>
#include <algorithm>

/*
* 一个简单异步httpclientDemo，通过输入网址、path、请求方式，来获取并呈现Response
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
    transform(sFlag.begin(), sFlag.end(), sFlag.begin(), ::toupper);    //转成大写
    if (sFlag == "YES")
    {
        while (true)
        {
            {
                //std::unique_lock<std::mutex> guard(g_mutex1);         //？ 好像这里不需要用到信号量
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
            transform(sFlag.begin(), sFlag.end(), sFlag.begin(), ::toupper);    //转成大写
            if (sFlag == "NO")
            {
                myHttpClient.endWork();
                break;
            }
            //简单输入Request必备的信息
            std::cout << "Please input the host:" << std::endl;
            getline(std::cin, sHost);
            std::cout << "Please input the path:" << std::endl;
            getline(std::cin, sPath);
            std::cout << "Please input the request method:" << std::endl;
            getline(std::cin, sMethod);
            transform(sMethod.begin(), sMethod.end(), sMethod.begin(), ::toupper);    //转成大写

            myHttpClient.myRequest(myHttpResultHandler, sHost, 80, sPath, sMethod, mCustomHeaders, sBpdy, sResponse);

        }
    }

    return 0;
}
