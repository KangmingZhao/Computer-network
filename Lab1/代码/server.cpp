#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <ctime>
#include<map>

#pragma comment(lib, "ws2_32.lib") // socket��

using namespace std;

#define PORT 8080                       // �˿ں�
#define MaxClient 5                     // ���������
#define _CRT_SECURE_NO_WARNINGS         // ��ֹʹ�ò���ȫ�ĺ�������
#define _WINSOCK_DEPRECATED_NO_WARNINGS // ��ֹʹ�þɰ汾�ĺ�������
#define ipAddr "127.0.0.1"
#define RecvBufSize 2048 // ��������С
#define SendBufSize 2048


SOCKET clientSocket[MaxClient];    // �ͻ���socket���� �������ÿ���̵߳�socket
SOCKET serverSocket;                // ��������socket
SOCKADDR_IN clientAddrs[MaxClient]; // �ͻ��˵�ַ����
SOCKADDR_IN serverAddr;             // �����������ַ

int connect_num = 0; // ��ǰ���ӵĿͻ���
int connectCondition[MaxClient] = {}; // ÿһ�����ӵ����
map<SOCKET, string>client;
// ����Ч��
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_UNDERLINE   "\x1b[4m"
#define ANSI_RESET   "\x1b[0m"
DWORD WINAPI ThreadFunction(LPVOID lpParameter);

int check() // ��ѯ���е����ӿ�
{
    for (int i = 0; i < MaxClient; i++)
    {
        if (connectCondition[i] == 0) // ���ӿ���
        {
            return i;
        }
    }
    exit(EXIT_FAILURE);
}

int main()
{
    // ��ʼ��WinSock��
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
        cout << "��ʼ�� WinSockʧ��" << endl;
    }
    else {
        cout << "��ʼ�� WinSock �ɹ�" << endl;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    /*
    AF_INET��ʹ��ipv4��AF��ָ����ַ��ĺ꣬Address Family����Ҳ����ʹ��PF_INET,��ʹ��Socket API�����׽��ֱ��ʱ�����ǵȼ۵�
    SOCK_STREAM���׽������ͣ���֤�����ݵ������ԣ�ȷ�������ݵ������ԺͿɿ��ԣ��Լ��ṩ����ʽ��������ԣ�ȷ����������ݰ��շ���˳�򱻽���
                 ͬʱ����ʵ��Ҫ���һ����
    IPPROTO_TCP��ʹ��TCP����Э��
    */

    if (serverSocket == INVALID_SOCKET) // ������
    {
        cout << "���� Socket ����" << endl;
        return 0;
    }
    cout << "���� Socket �ɹ�" << endl;

    // ��ip��ַ�ͷ�������ַ
    serverAddr.sin_family = AF_INET;   // ��ַ����
    serverAddr.sin_port = htons(PORT); // �˿ں�
    inet_pton(AF_INET, ipAddr, &(serverAddr.sin_addr));
    if (bind(serverSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) // ���������׽������������ַ�Ͷ˿ڰ�
    {
        cout << "�׽�����˿ڰ�ʧ��" << endl;
        return 0;
    }
    else
    {
        cout << "�׽�����˿� " ANSI_COLOR_MAGENTA << PORT << ANSI_RESET " �󶨳ɹ�" << endl;
    }

    // ���ü���/�ȴ�����
    if (listen(serverSocket, MaxClient) != 0)
    {
        cout << "���ü���ʧ��" << endl;
        return 0;
    }
    else
    {
        cout << "���ü����ɹ�" << endl;
    }

    cout << "----------�ȴ�����-------------" << endl;
    // ѭ�����տͻ�������
    while (true)
    {
        if (connect_num < MaxClient)
        {
            int num = check();
            int addrlen = sizeof(SOCKADDR);
            clientSocket[num] = accept(serverSocket, (sockaddr*)&clientAddrs[num], &addrlen); // �ȴ��ͻ�������

            // ��ȡ�ͻ���ip��ַ
            char clientIp[INET_ADDRSTRLEN] = "";
            inet_ntop(AF_INET, &(clientAddrs[num].sin_addr), clientIp, INET_ADDRSTRLEN);

            if (clientSocket[num] == SOCKET_ERROR)
            {
                perror("�ͻ��˳��� \n");
                closesocket(serverSocket);
                WSACleanup();
                exit(EXIT_FAILURE);
            }
            connectCondition[num] = 1;// ����λ��1��ʾռ��
            connect_num++; // ��ǰ��������1

            // ����ʱ�������¼��ǰͨѶʱ��
            auto currentTime = chrono::system_clock::now();
            time_t timestamp = chrono::system_clock::to_time_t(currentTime);
            tm localTime;
            localtime_s(&localTime, &timestamp);
            char timeStr[50];
            HANDLE Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadFunction, (LPVOID)num, 0, NULL); // �����߳�
        
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &localTime); // ��ʽ��ʱ��
           
            cout << "��ǰ�ͻ�����������Ϊ: " ANSI_COLOR_CYAN << connect_num << ANSI_RESET <<" "<<ANSI_COLOR_GREEN << timeStr << ANSI_RESET << endl;

            if (Thread == NULL) // �̴߳���ʧ��
            {
                perror("�̴߳���ʧ��\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                CloseHandle(Thread);
            }
        }
        else
        {
            cout << "�ͻ�����������" << endl << endl;
        }
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

DWORD WINAPI ThreadFunction(LPVOID lpParameter) // �̺߳���
{

    int recvlen = 0;
    char RecvBuf[RecvBufSize];
    char SendBuf[SendBufSize];
    //SOCKET c = (SOCKET)lpParameter;// Ϊ��ǰ���ӽ�������
    int index = (int)lpParameter;
    //recvlen = recv(clientSocket[index], RecvBuf, sizeof(RecvBuf), 0);
    char username[1024] = { 0 };
    recvlen = recv(clientSocket[index], username, sizeof(username), 0);
    client[clientSocket[index]] = string(username);
    string bufsend;
    bufsend = "��ӭ[" + client[clientSocket[index]] + "]���������ң�";
    // ��������Ϣͬ���������ͻ���
    for (int i = 0; i < MaxClient; i++)
    {
        // ע�� ��ʱ��Ҫ���Ƿ����� ����Ҫ��connectCondition

        if (connectCondition[i] == 1)
        {
            send(clientSocket[i], bufsend.data(), sizeof(bufsend), 0);
        }
    }
    while (1)
    {
        
        recvlen = 0;
        recvlen = recv(clientSocket[index], RecvBuf, sizeof(RecvBuf), 0);
        if (recvlen > 0)
        {
            //����ʱ�������¼��ǰͨѶʱ��
            auto currentTime = chrono::system_clock::now();
            time_t timestamp = chrono::system_clock::to_time_t(currentTime);
            tm localTime;
            localtime_s(&localTime, &timestamp);
            char timeStr[50];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d-%H:%M:%S", &localTime); // ��ʽ��ʱ��
            string str = "[" + client[clientSocket[index]] + "]: ";
            cout << ANSI_COLOR_YELLOW<<str<<ANSI_RESET<< RecvBuf<< " -ʱ�䣺" << timeStr << endl;
            sprintf_s(SendBuf, sizeof(SendBuf), "%s %s -ʱ�䣺%s ", str.data(), RecvBuf,timeStr); // ��ʽ��������Ϣ
            // ��������Ϣͬ���������ͻ���
            for (int i = 0; i < MaxClient; i++)
            {
                // ע�� ��ʱ��Ҫ���Ƿ����� ����Ҫ��connectCondition

                if (connectCondition[i] == 1)
                {
                    send(clientSocket[i], SendBuf, sizeof(SendBuf), 0);
                }
            }


        }
        else
        {
            if (GetLastError() == 10054)  // �ر�����
            {
                // ʱ����� 
                auto currentTime = chrono::system_clock::now();
                time_t timestamp = chrono::system_clock::to_time_t(currentTime);
                tm localTime;
                localtime_s(&localTime, &timestamp);
                char timeStr[50];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d-%H:%M:%S", &localTime); // ��ʽ��ʱ��
                string str = "[" + client[clientSocket[index]] + "]";
                cout <<ANSI_COLOR_GREEN<<str<<ANSI_RESET << " ���˳�! -ʱ��: " << timeStr << endl;
                string strsend;
                strsend = str + "���˳�!";
                // sprintf_s(SendBuf, sizeof(SendBuf), "%s %s -ʱ�䣺%s ", str.data(), RecvBuf,timeStr); // ��ʽ��������Ϣ
            // ��������Ϣͬ���������ͻ���
            for (int i = 0; i < MaxClient; i++)
            {
                // ע�� ��ʱ��Ҫ���Ƿ����� ����Ҫ��connectCondition

                if (connectCondition[i] == 1)
                {
                    send(clientSocket[i], strsend.data(), sizeof(strsend), 0);
                }
            }

                closesocket(clientSocket[index]);
                connect_num--;
                connectCondition[index] = 0;// ��Ϊδ����
                return 0;
            }
            else
            {
                cout << "����ʧ��" << endl;
                break;

            }

        }

    }
}
