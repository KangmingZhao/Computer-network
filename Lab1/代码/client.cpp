#include<iostream>
// ����������Ҫ�Ŀ�
#include<WinSock2.h>
#include<WS2tcpip.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib,"ws2_32.lib")   //socket��
#include<cstring>
using namespace std;
#define IP "127.0.0.1"
// ����socket 
SOCKET sockClient;// �ͻ���socket

// ������Ϣ���߳�
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
			cout << "������ѶϿ�" << endl;
			break;
		}
	}
	return 0;
}

int main()
{
	// ��ʼ�� Windows Sockets API
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) //�ɹ�����0
	{
		cout << "��ʼ��WinSock��ʧ��" << endl;
		return 0;
	}
	else
	{
		cout << "��ʼ��WinSock��ɹ���" << endl;
	}
	// 1.�����þ��
	sockClient = socket(AF_INET, SOCK_STREAM, 0);
	if (sockClient == INVALID_SOCKET)
	{
		cout << "����Socketʧ��" << endl;
		WSACleanup();
		return 0;
	}
	else
	{
		cout << "�����׽��ֳɹ�" << endl;
	}
	/*
	 ѧϰ��
	 - socket ���������ڴ����׽��ֵĺ���
	 - sockClient ���洢�������׽��ֵľ��
	 - AF_INET �ǵ�ַ�壨Address Family������ʾʹ�� IPv4 ��ַ�壬���� Internet ��ַ
	 - �ڶ���������ʾʹ����ʧ�׽��֣���TCPЭ��
	 - ʹ��TCPʱ����������Ϊ0
	*/
	// 2.��������
	SOCKADDR_IN serverAddr;//����˵�ַ
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	//serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	inet_pton(AF_INET, IP, &(serverAddr.sin_addr));
	/* IP ��ַ�ַ���ת��Ϊ�����Ʊ�ʾ����������洢�� serverAddr.sin_addr �У�
	�Ա��ں������׽��ֲ�����ʹ�á���ͨ�����ڽ�����ɶ��� IP ��ַ
	���� "127.0.0.1"��ת��Ϊ�׽�����Ҫ�Ķ����Ƹ�ʽ��
	*/
	//serverAddr.sin_addr.s_addr = inet_addr("localhost");
	if (connect(sockClient, (SOCKADDR*)&serverAddr, sizeof(sockaddr_in))==SOCKET_ERROR)
	{
		cout << "���������ʧ�ܣ�" << WSAGetLastError() << endl;
		closesocket(sockClient);
		WSACleanup();
		return 0;
	}
	else
	{
		cout << "��������ӳɹ���" << endl;
		
	}
	//3.������Ϣ�߳�֮�����ڽ�����Ϣ��
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, NULL, 0, 0);
	char msg[1024];
	memset(msg, 0, sizeof(msg));
	cout << "��ʼ������Ϣ������exit�Ͽ������˵�����" << endl;
	char username[1024];
	cout << "�����������û����� ";
	cin.getline(username, sizeof(username));
	//��������
	send(sockClient, username, sizeof(username), 0);
	// �������ڷ�����Ϣ
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
	//�ر�socket
	closesocket(sockClient);
	WSACleanup();
	return 0;

}