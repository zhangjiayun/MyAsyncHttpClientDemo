#pragma once
//该头文件用于实现跨平台windows、linux的定义
//用一套代码在两个平台都能编译

#ifdef WINDOWS  //如果是windows平台

#define MSG_NOSIGNAL 0      //windows平台没有但linux上有该宏定义，因此给windows也定义上，值为0

#else

typedef int SOCKET;     //给linux起一个int类型的别名，跟windows保持一致
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define closesocket(s) close(s)  //windows和linux平台都有close(s)函数，linux的close用于关闭套接字，但windows用来关闭套接字的是
//closesocket(s)函数而不是close(s)，因此给linux定义一个宏closesocket，跟windows保持一致

#endif

#define SERV_PORT        80         /* TCP and UDP */
