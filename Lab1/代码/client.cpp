#include<iostream>
// 网络编程所需要的库
#include<WinSock2.h>
#include<WS2tcpip.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib,"ws2_32.lib")   //socket库
#include<cstring>
using namespace std;
#define IP "127.0.0.1"
// 申请socket 
SOCKET sockClient;// 客户端socket

// 接收消息的线程
DWORD WINAPI  RecvThread()
{
	while (1)
	{
		char msgbuf[2048];
		memset(msgbuf, 0, sizeof(msgbuf));

		if (recv(sockClient, msgbuf, sizeof(msgbuf), 0) > 0)
		{


			cout << endl;
			cout << msgbuf << endl;
		}
		else if (recv(sockClient, msgbuf, sizeof(msgbuf), 0) == 0)
		{
			cout << endl;
			cout << "服务端已断开" << endl;
			break;
		}
	}
	return 0;
}

int main()
{
	// 初始化 Windows Sockets API
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) //成功返回0
	{
		cout << "初始化WinSock库失败" << endl;
		return 0;
	}
	else
	{
		cout << "初始化WinSock库成功！" << endl;
	}
	// 1.创立好句柄
	sockClient = socket(AF_INET, SOCK_STREAM, 0);
	if (sockClient == INVALID_SOCKET)
	{
		cout << "创建Socket失败" << endl;
		WSACleanup();
		return 0;
	}
	else
	{
		cout << "创建套接字成功" << endl;
	}
	/*
	 学习：
	 - socket 函数是用于创建套接字的函数
	 - sockClient 将存储创建的套接字的句柄
	 - AF_INET 是地址族（Address Family），表示使用 IPv4 地址族，用于 Internet 地址
	 - 第二个参数表示使用流失套接字，即TCP协议
	 - 使用TCP时第三个参数为0
	*/
	// 2.建立连接
	SOCKADDR_IN serverAddr;//服务端地址
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	//serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	inet_pton(AF_INET, IP, &(serverAddr.sin_addr));
	/* IP 地址字符串转换为二进制表示，并将结果存储在 serverAddr.sin_addr 中，
	以便在后续的套接字操作中使用。这通常用于将人类可读的 IP 地址
	（如 "127.0.0.1"）转换为套接字需要的二进制格式。
	*/
	//serverAddr.sin_addr.s_addr = inet_addr("localhost");
	if (connect(sockClient, (SOCKADDR*)&serverAddr, sizeof(sockaddr_in))==SOCKET_ERROR)
	{
		cout << "服务端连接失败！" << WSAGetLastError() << endl;
		closesocket(sockClient);
		WSACleanup();
		return 0;
	}
	else
	{
		cout << "服务端连接成功！" << endl;
		
	}
	//3.创建消息线程之后用于接收信息呃
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, NULL, 0, 0);
	char msg[1024];
	memset(msg, 0, sizeof(msg));
	cout << "开始发送消息！输入exit断开与服务端的连接" << endl;
	char username[1024];
	cout << "请输入您的用户名： ";
	cin.getline(username, sizeof(username));
	//发送名字
	send(sockClient, username, sizeof(username), 0);
	// 这里用于发送信息
	while (1)
	{
		cin.getline(msg, sizeof(msg));
		if (strcmp(msg, "exit") == 0)
		{
			break;
		}
		//char message[2048];
		//sprintf_s(message, "%s: %s", username, msg);
		//send(sockClient, message, sizeof(message), 0);

		send(sockClient, msg, sizeof(msg), 0);
	}
	//关闭socket
	closesocket(sockClient);
	WSACleanup();
	return 0;

}