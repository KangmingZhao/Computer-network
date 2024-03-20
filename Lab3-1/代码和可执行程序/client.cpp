#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <ctime>
#include<map>
#pragma comment(lib, "ws2_32.lib") // socket库
using namespace std;
#define PORT 8000
#define _CRT_SECURE_NO_WARNINGS         // 禁止使用不安全的函数报错
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁止使用旧版本的函数报错
#define IP "127.0.0.1"
#define MAX_TIME 0.5*CLOCKS_PER_SEC


const int MAXSIZE = 1024;//传输缓冲区最大长度
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1，FIN = 0
const unsigned char SYN_ACK = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;
const unsigned char FINAL_CHECK = 0x20;//FC=1.FIN=0,OVER=0,FIN=0,ACK=0,SYN=0
const unsigned char OVER = 0x7;//结束标志
const unsigned char OVER_ACK = 0xA;
#define MAX_DATA_LENGTH 4096 // 假设最大数据长度为512字节，根据实际情况调整
clock_t connectClock;
clock_t start;
clock_t END_GLOBAL;
clock_t START_GLOBAL;
u_long unlockmode = 1;
u_long lockmode = 0;
char* message = new char[100000000];
unsigned long long int messagelength = 0;//最后一个要传的下标是多少
unsigned long long int messagepointer = 0;//下一个该传的位置
WSADATA wsadata;
//声明套接字需要绑定的地址
SOCKADDR_IN server_addr;
//声明路由器需要绑定的地址
SOCKADDR_IN router_addr;
//声明客户端的地址
SOCKADDR_IN client_addr;
SOCKET client;
int slen = sizeof(server_addr);
int rlen = sizeof(router_addr);
struct  HeadMsg
{
    u_short len;			// 数据长度16位
    u_short checkSum;		// 16位校验和
    u_short flag; // FIN ACK SYN  
    u_short ack;
    u_short seq;		// 序列号，可以表示0-255

    HeadMsg()
    {
        checkSum = 0;
        len = 0;
        seq = 0;
        flag = 0;
        ack = 0;
    }

};
u_short cal_ck_sum(u_short* msg, int size)
{

    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    memcpy(buf, msg, size);
    u_long sum = 0;
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}
void Initial();
int sendmessage();
int endsend();
bool ConnectClient(SOCKET& client, SOCKADDR_IN& serverAddr, int& serverAddrlen)
{
    HeadMsg header;
    // 1. 发送第一次握手
    //cout << "准备发送第一次握手" << endl;
    char* sendBuf = new char[sizeof(header) + 1];
    char* recvBuf = new char[sizeof(header) + 1];
    memset(sendBuf, 0, sizeof(sendBuf));

    // 设置头部信息
    header.flag = SYN;
    header.seq = 0;
    header.len = 0;
    header.checkSum = 0;
    u_short sum = cal_ck_sum((u_short*)&header, sizeof(header));
    header.checkSum = sum;
    // 封装信息
    memcpy(sendBuf, &header, sizeof(header));
FIRST_SHAKE:
    // 发送第一次握手
    if (sendto(client, sendBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, serverAddrlen) == -1) {
        cout << "客户端：第一次握手连接失败！" << endl;
        return false;
    }
    else {
        cout << "客户端：第一次握手连接成功！" << endl;
    }
    // 设置为非阻塞模式
    u_long mode = 1;
    ioctlsocket(client, FIONBIO, &mode);
    // 设定计时器
    connectClock = clock();
    // 2. 等待服务器响应第二次握手
    while (true) {
        if (recvfrom(client, recvBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, &serverAddrlen) > 0) {
            memcpy(&header, recvBuf, sizeof(header));
            if (header.flag == SYN_ACK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
                cout << "客户端: 第二次握手连接成功！" << endl;
                break;
            }
            else {
                return false;
            }
        }
        if (clock() - connectClock > 75 * CLOCKS_PER_SEC)
        {
            if (sendto(client, sendBuf, sizeof(header), 0, (sockaddr*)&serverAddr, serverAddrlen) == -1) {
                cout << "客户端：第一次握手请求发送失败..." << endl;
                return false;
            }
            start = clock();
            cout << "客户端：第一次握手消息反馈超时,正在重新发送" << endl;
            goto FIRST_SHAKE;
        }
    }

    // 3. 发送第三次握手
    header.flag = ACK;
    header.checkSum = 0;
    header.seq = 1;
    sum = cal_ck_sum((u_short*)&header, sizeof(header));
    header.checkSum = sum;

    // 封装信息
    memcpy(sendBuf, &header, sizeof(header));

    // 发送第三次握手
THIRD_SHAKE:
    if (sendto(client, sendBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == -1) {

        cout << "客户端：第三次握手发送失败" << endl;
        return false;
    }
    else
    {
        cout << "客户端：第三次握手发送成功" << endl;
    }


    return true;
}
void log();
int DisconnectClient(SOCKET& client, SOCKADDR_IN& serverAddr, int& addrlen)
{
    HeadMsg header;
    char* sendBuf = new char[sizeof(header) + 1];
    char* recvBuf = new char[sizeof(header) + 1];

    // 第一次挥手：客户端发送断开连接请求
    header.flag = FIN;         // FIN
    header.checkSum = 0;
    header.seq = 0;          // 设置序列号
    // 计算校验和
    header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
    // 封装信息
    memcpy(sendBuf, &header, sizeof(header));
WAVE_1:
    // 发送第一次挥手
    if (sendto(client, sendBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == -1) {
        return 0;
    }
    else
    {
        cout << "客户端：第一次挥手发送成功" << endl;
    }
WAVE_2:
    u_long mode = 0;  // 0 表示阻塞模式，1 表示非阻塞模式
    ioctlsocket(client, FIONBIO, &mode);
    // 第二次挥手：等待服务器响应
    clock_t start = clock();
    while (recvfrom(client, recvBuf, sizeof(header), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {

    }
    if (clock() - start > 75 * MAX_TIME) {

        if (sendto(client, sendBuf, sizeof(header), 0, (sockaddr*)&serverAddr, addrlen) == -1) {
            cout << "客户端：第一次挥手发送失败" << endl;
            return -1;
        }
        cout << "客户端：第一次挥手反馈接收超时，重发第一次挥手" << endl;
        start = clock();
        goto WAVE_1;

    }
    memcpy(&header, recvBuf, sizeof(header));
    if (header.flag == ACK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
        //cout << cal_ck_sum((u_short*)&header, sizeof(header)) << endl;
        cout << "客户端：收到第二次挥手消息" << endl;
    }
    else {
        cout << "客户端：第二次挥手消息接受失败...." << endl;
        goto WAVE_2;
    }
    // 第三次 挥手等待服务器的FIN
WAVE_3:
    while (recvfrom(client, recvBuf, sizeof(header), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {}
    memcpy(&header, recvBuf, sizeof(header));
    if (header.flag == FIN && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
        cout << "客户端：收到第三次挥手消息" << endl;
    }
    else {
        cout << "客户端：第三次挥手消息接受失败,即将重新接收" << endl;
        goto WAVE_3;
    }
    header.seq = 1;
    header.flag = ACK;
    header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
    //cout <<header.flag<<" "<< cal_ck_sum((u_short*)&header, sizeof(header)) << endl;
    memcpy(sendBuf, &header, sizeof(header));
WAVE_4:
    if (sendto(client, sendBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == -1) {
        cout << "客户端：第四次挥手发送失败" << endl;
        return 0;
    }
    else
    {
        cout << "客户端：第四次挥手发送成功" << endl;
        return 0;
    }
    start = clock();
    ioctlsocket(client, FIONBIO, &unlockmode);
    while (recvfrom(client, recvBuf, sizeof(header), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
        if (clock() - start > 10 * MAX_TIME) {
            cout << "客户端：接受反馈超时，重发第四次挥手" << endl;
            goto WAVE_4;
        }
    }
    memcpy(&header, recvBuf, sizeof(header));
    if (header.flag == FINAL_CHECK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
        cout << "客户端：四次挥手完成,即将断开连接" << endl;
        return true;
    }
    else {
        cout << "客户端：数据包错误,准备重发第四次挥手" << endl;
        goto WAVE_4;
    }

    // 清理资源
    delete[] recvBuf;
    delete[] sendBuf;

    return true;
}
int main()
{

    /*------------------连接部分---------------------*/
    Initial();
    ConnectClient(client, server_addr, slen);
    /*------------------发送消息---------------------*/
     //以二进制方式打开文件
    string filename;
    cout << "请输入要传输的文件名" << endl;
    cin >> filename;
    ifstream fin(filename.c_str(), ifstream::binary);//以二进制方式打开文件
    unsigned long long int index = 0;
    unsigned char temp = fin.get();
    while (fin)
    {
        message[index++] = temp;
        temp = fin.get();
    }
    messagelength = index - 1;
    fin.close();
    cout << "完成文件读入工作" << endl;
    sendmessage();
    log();
    /*------------------挥手断开---------------------*/
    DisconnectClient(client, server_addr, rlen);
    // 关闭客户端套接字
    closesocket(client);
    // 释放 Winsock
    WSACleanup();
    return 0;
}

void Initial() {
    WSAStartup(MAKEWORD(2, 1), &wsadata);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000);
    server_addr.sin_addr.s_addr = htonl(2130706433);

    router_addr.sin_family = AF_INET;
    router_addr.sin_port = htons(8001);
    router_addr.sin_addr.s_addr = htonl(2130706433);

    //指定一个客户端
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8002);
    client_addr.sin_addr.s_addr = htonl(2130706433);

    client = socket(AF_INET, SOCK_DGRAM, 0);
    bind(client, (SOCKADDR*)&client_addr, sizeof(client_addr));
    cout << "初始完毕" << endl;
}

int sendmessage() {
    START_GLOBAL = clock();
    //设置是否为非阻塞模式
    ioctlsocket(client, FIONBIO, &unlockmode);
    HeadMsg header;
    char* recvbuffer = new char[sizeof(header)];
    char* sendbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];
    int currentSeq = 0;
    while (true) {

        int send_length;//数据传输长度
        if (messagepointer > messagelength) {
            if (endsend() == 1) { return 1; }
            return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH) {
            send_length = MAX_DATA_LENGTH;
        }
        else {
            send_length = messagelength - messagepointer + 1;//需要计算发送的长度
        }


        header.seq = currentSeq;
        header.len = send_length;
        header.checkSum = 0;
        memset(sendbuffer, 0, sizeof(header) + MAX_DATA_LENGTH);
        memcpy(sendbuffer, &header, sizeof(header));            // 拷贝 header 内容
        memcpy(sendbuffer + sizeof(header), message + messagepointer, send_length);
        messagepointer += send_length;
        header.checkSum = cal_ck_sum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH);
        memcpy(sendbuffer, &header, sizeof(header));
        // 发送数据包
 send:
        if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
            cout << "数据包发送失败！" << endl;
            return -1;
        }
        else
        {
            cout << "成功发送数据包，序列号: " << header.seq << " ACK: " << header.ack << " 校验和: " << header.checkSum << endl;
        }

        // 接收 ACK
wait:
        clock_t start = clock();
        while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) < 0) {
        
            if (clock() - start > MAX_TIME)
            {
                //超时 重传
                cout << "未收到ACK，即将超时重传" << endl;
                if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
                    cout << "数据包发送失败！" << endl;
                    return -1;
                }
                else
                {
                    cout << "成功发送数据包，序列号: " << header.seq << " ACK: " << header.ack << " 校验和: " << header.checkSum << endl;
                    goto wait;
                }
            }
        }
       
                bool receivedAck = false;
                 // 检查 ACK 是否正确
                memcpy(&header, recvbuffer, sizeof(header));
                int expectedAck = (currentSeq + 1) % 256;
                if (header.ack == expectedAck && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
                    cout << "成功接受服务端数据ACK反馈，准备发出下一个数据包" << endl;;
                    cout << "序列号: " << header.seq << " ACK: " << header.ack << " 校验和: " << header.checkSum << endl;
                    receivedAck = true;
                    currentSeq = (currentSeq + 1) % 256; // 更新序列号
                }
                else {
                    cout << "服务端未反馈正确的 ACK 数据包，正在等待重传" << endl;
                    goto send;
                }
            
        
    }
}

int endsend() {
    END_GLOBAL = clock();
    HeadMsg header;
    char* sendbuffer = new char[sizeof(header)];
    char* recvbuffer = new char[sizeof(header)];
    header.checkSum = 0;
    header.flag = OVER;
    header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
SEND:
    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "结束信号发送失败" << endl;
        return -1;
    }
    cout << "结束信号发送成功" << endl;
    clock_t start = clock();
RECV:
    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "结束信号发送失败" << endl;
                return -1;
            }
            start = clock();
            cout << "结束信号反馈超时,即将重传" << endl;
            goto SEND;
        }
    }
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == OVER_ACK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
        cout << "结束消息发送成功，即将完成数据发送" << endl;
        return 1;
    }
    else {
        cout << "数据包错误，即将重传" << endl;
        goto RECV;
    }
}
void log() {
    cout << "*********传输日志********" << endl;
    cout << "**************************" << endl;
    cout << "本次传输报文总长度：" << messagepointer << "字节" << endl;
    cout << "共有：" << (messagepointer / 256) + 1 << "个报文段分别转发" << endl;

    double transferTime = static_cast<double>(END_GLOBAL - START_GLOBAL) / CLOCKS_PER_SEC;
    cout << "本次传输时间：" << transferTime << "秒" << endl;

    if (transferTime > 0) {
        double throughput = static_cast<double>(messagepointer) / transferTime;
        cout << "本次传输吞吐率：" << throughput << "字节/秒" << endl;
    }
    else {
        cout << "本次传输吞吐率无法计算（传输时间为零）" << endl;
    }
}
