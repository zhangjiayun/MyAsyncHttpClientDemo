#pragma once
//��ͷ�ļ�����ʵ�ֿ�ƽ̨windows��linux�Ķ���
//��һ�״���������ƽ̨���ܱ���

#ifdef WINDOWS  //�����windowsƽ̨

#define MSG_NOSIGNAL 0      //windowsƽ̨û�е�linux���иú궨�壬��˸�windowsҲ�����ϣ�ֵΪ0

#else

typedef int SOCKET;     //��linux��һ��int���͵ı�������windows����һ��
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define closesocket(s) close(s)  //windows��linuxƽ̨����close(s)������linux��close���ڹر��׽��֣���windows�����ر��׽��ֵ���
//closesocket(s)����������close(s)����˸�linux����һ����closesocket����windows����һ��

#endif

#define SERV_PORT        80         /* TCP and UDP */
