#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <ctime>
#include<map>

#pragma comment(lib, "ws2_32.lib") // socket库

using namespace std;

#define PORT 8080                       // 端口号
#define MaxClient 5                     // 最大连接数
#define _CRT_SECURE_NO_WARNINGS         // 禁止使用不安全的函数报错
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁止使用旧版本的函数报错
#define ipAddr "127.0.0.1"
#define RecvBufSize 2048 // 缓冲区大小
#define SendBufSize 2048


SOCKET clientSocket[MaxClient];    // 客户端socket数组 用来存放每个线程的socket
SOCKET serverSocket;                // 服务器端socket
SOCKADDR_IN clientAddrs[MaxClient]; // 客户端地址数组
SOCKADDR_IN serverAddr;             // 定义服务器地址

int connect_num = 0; // 当前连接的客户数
int connectCondition[MaxClient] = {}; // 每一个连接的情况
map<SOCKET, string>client;
// 字体效果
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_UNDERLINE   "\x1b[4m"
#define ANSI_RESET   "\x1b[0m"
DWORD WINAPI ThreadFunction(LPVOID lpParameter);

int check() // 查询空闲的连接口
{
    for (int i = 0; i < MaxClient; i++)
    {
        if (connectCondition[i] == 0) // 连接空闲
        {
            return i;
        }
    }
    exit(EXIT_FAILURE);
}

int main()
{
    // 初始化WinSock库
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
        cout << "初始化 WinSock失败" << endl;
    }
    else {
        cout << "初始化 WinSock 成功" << endl;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    /*
    AF_INET：使用ipv4（AF是指定地址族的宏，Address Family），也可以使用PF_INET,在使用Socket API进行套接字编程时二者是等价的
    SOCK_STREAM：套接字类型，保证了数据的有序性，确保了数据的完整性和可靠性，以及提供了流式传输的特性，确保传输的数据按照发送顺序被接收
                 同时还是实验要求的一部分
    IPPROTO_TCP：使用TCP传输协议
    */

    if (serverSocket == INVALID_SOCKET) // 错误处理
    {
        cout << "创建 Socket 错误" << endl;
        return 0;
    }
    cout << "创建 Socket 成功" << endl;

    // 绑定ip地址和服务器地址
    serverAddr.sin_family = AF_INET;   // 地址类型
    serverAddr.sin_port = htons(PORT); // 端口号
    inet_pton(AF_INET, ipAddr, &(serverAddr.sin_addr));
    if (bind(serverSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) // 将服务器套接字与服务器地址和端口绑定
    {
        cout << "套接字与端口绑定失败" << endl;
        return 0;
    }
    else
    {
        cout << "套接字与端口 " ANSI_COLOR_MAGENTA << PORT << ANSI_RESET " 绑定成功" << endl;
    }

    // 设置监听/等待队列
    if (listen(serverSocket, MaxClient) != 0)
    {
        cout << "设置监听失败" << endl;
        return 0;
    }
    else
    {
        cout << "设置监听成功" << endl;
    }

    cout << "----------等待连接-------------" << endl;
    // 循环接收客户端请求
    while (true)
    {
        if (connect_num < MaxClient)
        {
            int num = check();
            int addrlen = sizeof(SOCKADDR);
            clientSocket[num] = accept(serverSocket, (sockaddr*)&clientAddrs[num], &addrlen); // 等待客户端请求

            // 获取客户端ip地址
            char clientIp[INET_ADDRSTRLEN] = "";
            inet_ntop(AF_INET, &(clientAddrs[num].sin_addr), clientIp, INET_ADDRSTRLEN);

            if (clientSocket[num] == SOCKET_ERROR)
            {
                perror("客户端出错 \n");
                closesocket(serverSocket);
                WSACleanup();
                exit(EXIT_FAILURE);
            }
            connectCondition[num] = 1;// 连接位置1表示占用
            connect_num++; // 当前连接数加1

            // 创建时间戳，记录当前通讯时间
            auto currentTime = chrono::system_clock::now();
            time_t timestamp = chrono::system_clock::to_time_t(currentTime);
            tm localTime;
            localtime_s(&localTime, &timestamp);
            char timeStr[50];
            HANDLE Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadFunction, (LPVOID)num, 0, NULL); // 创建线程
        
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &localTime); // 格式化时间
           
            cout << "当前客户端连接数量为: " ANSI_COLOR_CYAN << connect_num << ANSI_RESET <<" "<<ANSI_COLOR_GREEN << timeStr << ANSI_RESET << endl;

            if (Thread == NULL) // 线程创建失败
            {
                perror("线程创建失败\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                CloseHandle(Thread);
            }
        }
        else
        {
            cout << "客户端数量已满" << endl << endl;
        }
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

DWORD WINAPI ThreadFunction(LPVOID lpParameter) // 线程函数
{

    int recvlen = 0;
    char RecvBuf[RecvBufSize];
    char SendBuf[SendBufSize];
    //SOCKET c = (SOCKET)lpParameter;// 为当前连接建立索引
    int index = (int)lpParameter;
    //recvlen = recv(clientSocket[index], RecvBuf, sizeof(RecvBuf), 0);
    char username[1024] = { 0 };
    recvlen = recv(clientSocket[index], username, sizeof(username), 0);
    client[clientSocket[index]] = string(username);
    string bufsend;
    bufsend = "欢迎[" + client[clientSocket[index]] + "]加入聊天室！";
    // 将所有信息同步到各个客户端
    for (int i = 0; i < MaxClient; i++)
    {
        // 注意 此时需要看是否被连接 故需要用connectCondition

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
            //创建时间戳，记录当前通讯时间
            auto currentTime = chrono::system_clock::now();
            time_t timestamp = chrono::system_clock::to_time_t(currentTime);
            tm localTime;
            localtime_s(&localTime, &timestamp);
            char timeStr[50];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d-%H:%M:%S", &localTime); // 格式化时间
            string str = "[" + client[clientSocket[index]] + "]: ";
            cout << ANSI_COLOR_YELLOW<<str<<ANSI_RESET<< RecvBuf<< " -时间：" << timeStr << endl;
            sprintf_s(SendBuf, sizeof(SendBuf), "%s %s -时间：%s ", str.data(), RecvBuf,timeStr); // 格式化发送信息
            // 将所有信息同步到各个客户端
            for (int i = 0; i < MaxClient; i++)
            {
                // 注意 此时需要看是否被连接 故需要用connectCondition

                if (connectCondition[i] == 1)
                {
                    send(clientSocket[i], SendBuf, sizeof(SendBuf), 0);
                }
            }


        }
        else
        {
            if (GetLastError() == 10054)  // 关闭连接
            {
                // 时间输出 
                auto currentTime = chrono::system_clock::now();
                time_t timestamp = chrono::system_clock::to_time_t(currentTime);
                tm localTime;
                localtime_s(&localTime, &timestamp);
                char timeStr[50];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d-%H:%M:%S", &localTime); // 格式化时间
                string str = "[" + client[clientSocket[index]] + "]";
                cout <<ANSI_COLOR_GREEN<<str<<ANSI_RESET << " 已退出! -时间: " << timeStr << endl;
                string strsend;
                strsend = str + "已退出!";
                // sprintf_s(SendBuf, sizeof(SendBuf), "%s %s -时间：%s ", str.data(), RecvBuf,timeStr); // 格式化发送信息
            // 将所有信息同步到各个客户端
            for (int i = 0; i < MaxClient; i++)
            {
                // 注意 此时需要看是否被连接 故需要用connectCondition

                if (connectCondition[i] == 1)
                {
                    send(clientSocket[i], strsend.data(), sizeof(strsend), 0);
                }
            }

                closesocket(clientSocket[index]);
                connect_num--;
                connectCondition[index] = 0;// 置为未连接
                return 0;
            }
            else
            {
                cout << "接收失败" << endl;
                break;

            }

        }

    }
}
