#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <ctime>
#include<map>
#include<fstream>
#include<vector>
#include<mutex>
#pragma comment(lib, "ws2_32.lib") // socket库
using namespace std;

#define _CRT_SECURE_NO_WARNINGS         // 禁止使用不安全的函数报错
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁止使用旧版本的函数报错
#define ip "127.0.0.1"
#define MAX_TIME 0.5*CLOCKS_PER_SEC
#define MAX_DATA_LENGTH 4096
#define MAX_WAIT_TIME 5*CLOCKS_PER_SEC
WSADATA wsadata;
SOCKADDR_IN server_addr;
SOCKET server;
SOCKADDR_IN router_addr;;
SOCKADDR_IN client_addr;
int clen = sizeof(client_addr);
int rlen = sizeof(router_addr);
int recv_base = 1;
int WND = 8;
const int MAXSIZE = 1024;//传输缓冲区最大长度
// FINAL FIN OVER FIN ACY SYN
bool file_finish = false;
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1，FIN = 0
const unsigned char SYN_ACK = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;// FIN=1 ACK=1
const unsigned char FINAL_CHECK = 0x20;//FINAL_CHECK=1.FIN=0,OVER=0,FIN=0,ACK=0,SYN=0
const unsigned char OVER = 0x7;//结束标志
const unsigned char LAS=0x8;
const unsigned char OVER_ACK = 0xA;
const unsigned char DATA = 0x21;
char* message = new char[100000000];
unsigned long long int messagepointer = 0;
clock_t start;
u_long blockmode = 0;
u_long unblockmode = 1;
mutex buffer_lock;
mutex print_lock;
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
struct Packet {
    HeadMsg header;
    char Msg[MAX_DATA_LENGTH] = { 0 };
    Packet(HeadMsg h) :header(h) {};
};
struct Header_Recv
{
    Packet* packet;
    bool isRecv;

    Header_Recv(Packet* packet) : packet(packet), isRecv(true) {}
};
vector<Header_Recv*> Head_recv_vector;
void Initial();
int Connect(SOCKET& server, SOCKADDR_IN& clientAddr);
int receivemessage();
int endreceive();
int  Disconnect(SOCKET& server, SOCKADDR_IN& clientAddr);
void recv_file();
void update_recv_base();
int main()
{
    Initial();
    Connect(server, client_addr);
    thread receive_t(update_recv_base);
    //recv_file();
    receivemessage();
    receive_t.join();
    //Disconnect(server, client_addr);
    //receivemessage();
    string filename;
    cout << "请输入文件名称:";
    cin >> filename;
    ofstream fout(filename.c_str(), ofstream::binary);
    for (int i = 0; i < messagepointer; i++)
    {
        fout << message[i];
    }
    fout.close();
    cout << "文件已成功下载到本地" << endl;
    
    return 0;

}


int Connect(SOCKET& server, SOCKADDR_IN& clientAddr)
{
    HeadMsg header;
    u_short cksum;
    // 1.等待第一次握手
    clock_t time = clock();
    char* recvBuf = new char[sizeof(header) + 1];
    char* sendBuf = new char[sizeof(header) + 1];
    memset(recvBuf, 0, sizeof(recvBuf));
    int addrlen = sizeof(SOCKADDR);
    cout << "等待握手" << endl;
    // 第一次握手
    while (recvfrom(server, recvBuf, sizeof(header), 0, (SOCKADDR*)&clientAddr, &addrlen) < 0)
    {
        if (clock() - time > 75 * CLOCKS_PER_SEC) {
            cout << "服务端：连接超时,服务器自动断开" << endl;
            return -1;
        }
    }
    // 接受了第一次握手
    memcpy(&header, recvBuf, sizeof(header));
    if (header.flag == SYN && cal_ck_sum((u_short*)&header, sizeof(header)) == 0)
    {
        cout << "服务端： 收到客户端连接请求，第一次握手成功 " << endl;
    }
    else
    {
        cout << "服务端：等待重传" << endl;
    }
SHAKE_2:
    // 2. 主动发送第二次握手
    header.flag = SYN_ACK;
    header.checkSum = 0;
    header.len = 0;
    u_short sum = cal_ck_sum((u_short*)&header, sizeof(header));
    header.checkSum = sum;
    // 封装信息
    memcpy(sendBuf, &header, sizeof(header));
    if (sendto(server, sendBuf, sizeof(header), 0, (SOCKADDR*)&clientAddr, sizeof(SOCKADDR)) == -1) {

        cout << "服务端：第二次握手消息发送失败" << endl;
        return 0;
    }
    else {
        cout << "服务端：第二次握手消息发送成功" << endl;
    }
    // 3.接收客户端第三次握手
    clock_t start = clock();
    // 尚未接收到第三次握手
    while (recvfrom(server, recvBuf, sizeof(header), 0, (sockaddr*)&clientAddr, &addrlen) <= 0)
    {
        // 如果超时 则需要重发
        if ((clock() - start) > MAX_TIME)
        {

            if (sendto(server, sendBuf, sizeof(header), 0, (sockaddr*)&clientAddr, sizeof(SOCKADDR)) == -1)
            {
                cout << "服务端：第二次握手超时重新发送失败" << endl;
                return 0;
            }
            cout << "握手超时，第二次握手进行重传" << endl;
            start = clock();
        }
    }
    // 接收第三次握手成功
    HeadMsg cur;
    memcpy(&cur, recvBuf, sizeof(header));
    // 如果消息类型为ACK 且校验和为0 则
    if (cur.flag == ACK && cal_ck_sum((u_short*)&cur, sizeof(cur)) == 0)
    {
        cout << "服务端：第三次握手成功，连接成功" << endl;
    }
    // 如果没收到ACK 说明数据包有问题 那么将重发第二次握手 
    else
    {
        cout << "服务端：第三次握手接收数据包异常，等待重传" << endl;
        goto SHAKE_2;
    }

    return 0;
}
void update_recv_base()
{
    while (true) {
        buffer_lock.lock();
        // 使用迭代器遍历容器
        for (auto it = Head_recv_vector.begin(); it != Head_recv_vector.end(); /* no increment here */) {
            Header_Recv* hd_rec = *it;
            // 如果缓存中有当前基址的数据包，那么更新recv_base并删除该元素
            if (hd_rec->packet->header.seq == recv_base) {
                recv_base++;
                memcpy(message + messagepointer, hd_rec->packet->Msg,hd_rec->packet->header.len);
                messagepointer += hd_rec->packet->header.len;
                it = Head_recv_vector.erase(it); // 删除元素，并将迭代器指向下一个元素
                delete hd_rec; // 删除动态分配的资源（如果有的话）
            }
            else 
            {
                // 如果条件不满足，只需递增迭代器
                ++it;
            }
        }
        buffer_lock.unlock();
        if (file_finish&&Head_recv_vector.size()==0)
        {
            print_lock.lock();
            cout << "update_线程结束" << endl;
            print_lock.unlock();
            return;
        }
        //Sleep(200);
    }
    
}
int receivemessage() {
    HeadMsg header;
    char* recvbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];
    char* sendbuffer = new char[sizeof(header)];
    memset(recvbuffer, 0, MAX_DATA_LENGTH + sizeof(header));
    memset(sendbuffer, 0, sizeof(header));
    
    int expectedSeq = 1;
    int lastSendSeq = -1;
    while (true) {
        // Receive data
        while (true) {
            ioctlsocket(server, FIONBIO, &unblockmode);

            while (recvfrom(server, recvbuffer, sizeof(header) + MAX_DATA_LENGTH, 0, (sockaddr*)&router_addr, &rlen) <= 0) {
            
               
            }
            memcpy(&header, recvbuffer, sizeof(header));
            if (file_finish&&header.flag==FIN)
            {
                cout << "文件传输结束" << endl;
                Disconnect(server, router_addr);
                return 0;
            }
            if (header.seq >= recv_base && header.seq <= recv_base + WND && cal_ck_sum((u_short*)recvbuffer, sizeof(header) + header.len) == 0)
            {
                print_lock.lock();
                cout << "成功接收窗口内数据包";
                cout << "成功接收数据包，序列号: " << header.seq << " ACK: " << header.ack << " 校验和: " << header.checkSum << endl;
                print_lock.unlock();
                buffer_lock.lock();
                Packet* packet=new Packet(header);
                //memcpy(&packet->header, recvbuffer, sizeof(header));
                memcpy(&packet->Msg, recvbuffer + sizeof(header), header.len);
                Header_Recv* hr = new Header_Recv(packet);
                Head_recv_vector.push_back(hr);
                buffer_lock.unlock();
                if (header.flag == LAS)
                {
                    file_finish = true;
                }
                HeadMsg reply;
                reply.seq = header.seq;
                reply.ack = header.seq;
                reply.flag = ACK;
                //lastSendSeq = header.seq;// 记录上一次发出的包的ack
                reply.checkSum = 0;
                reply.checkSum = cal_ck_sum((u_short*)&reply, sizeof(header));
                memcpy(sendbuffer, &reply, sizeof(reply));
                if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                    cout << "ACK发送失败" << endl;
                    return -1;
                }
                else {
                    cout << "ACK发送成功：" << "序列号: " << header.seq << " ACK: " << header.ack << " 校验和: " << header.checkSum << endl;
                }
                break;
           
            }
            else
            {
                cout << "接收到已接受过的数据包";
                cout << "重发ACK";
                header.ack = header.seq;
                header.flag = ACK;
                //lastSendSeq = header.seq;// 记录上一次发出的包的ack
                header.checkSum = 0;
                header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
                memcpy(sendbuffer, &header, sizeof(header));
                if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                    cout << "ACK重新发送失败" << endl;
                    return -1;
                }
                else {
                    cout << "ACK重新发送成功：" << "序列号: " << header.seq << " ACK: " << header.ack << " 校验和: " << header.checkSum << endl;
                }
            }
        
        }
        //if (file_finish)
        //{
        //    
        //    //Sleep(1000);
        //    cout << "文件接收完毕" << endl;
        //    return 0;
        //    
        //}
      
    }
}

int endreceive() {
    HeadMsg header;
    char* sendbuffer = new char[sizeof(header)];
    header.flag = OVER_ACK;
    header.checkSum = 0;
    header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));

    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) >= 0) {
        cout << "结束确认消息发送成功" << endl;
        return 1;
    }
    else {
        cout << "结束确认消息发送失败" << endl;
        return 0;
    }
}
int  Disconnect(SOCKET& server, SOCKADDR_IN& clientAddr)
{
    HeadMsg header;
    char* recvBuf = new char[sizeof(header) + 1];
    char* sendBuf = new char[sizeof(header) + 1];
    memset(recvBuf, 0, sizeof(header) + 1);
    memcpy(&header, recvBuf, sizeof(header));
    int addrlen = sizeof(SOCKADDR);
WAVE_1:
    // 1. 等待来自客户端的FIN请求
    //if (recvfrom(server, recvBuf, sizeof(header), 0, (SOCKADDR*)&clientAddr, &addrlen) > 0) {
        //memcpy(&header, recvBuf, sizeof(header));
        //if (header.flag == FIN && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
            cout << "服务器: 收到第一次挥手信息" << endl;
        //}
        /*else {
          
            cout << "服务器:第一次挥手接收失败" << endl;
            goto WAVE_1;
        }*/

WAVE_2:
    // 2. 发送ACK响应以确认客户端的FIN
    header.flag = ACK;
    header.checkSum = 0;
    header.seq = 2;
    u_short sum = cal_ck_sum((u_short*)&header, sizeof(header));
    header.checkSum = sum;
    memcpy(sendBuf, &header, sizeof(header));

    if (sendto(server, sendBuf, sizeof(header), 0, (SOCKADDR*)&clientAddr, sizeof(SOCKADDR)) == -1) {
        return false;
    }
    else
    {
        cout << "服务端：第二次挥手发送成功" << endl;
    }
WAVE_3:
    // 3. 发送FIN请求以从服务器端主动断开连接
    header.flag = FIN;
    header.checkSum = 0;
    header.seq = 3;
    sum = cal_ck_sum((u_short*)&header, sizeof(header));
    header.checkSum = sum;
    memcpy(sendBuf, &header, sizeof(header));
    if (sendto(server, sendBuf, sizeof(header), 0, (SOCKADDR*)&clientAddr, sizeof(SOCKADDR)) == -1) {
        return false;
    }
    else
    {
        cout << "服务端：第三次挥手发送成功" << endl;
    }
    ioctlsocket(server, FIONBIO, &unblockmode);
    start = clock();
    // 4. 等待来自客户端的ACK响应
    while (recvfrom(server, recvBuf, sizeof(header), 0, (sockaddr*)&clientAddr, &addrlen) <= 0) {
        if (clock() - start > 75*MAX_TIME) {
            cout << "第四次挥手消息接收延迟，.准备重发二三次挥手" << endl;
            ioctlsocket(server, FIONBIO, &blockmode);
            goto WAVE_2;
        }
    }
    memcpy(&header, recvBuf, sizeof(header));
    //cout << "ACK: " << header.flag << header.seq << endl;
    if (header.flag == ACK&&cal_ck_sum((u_short*)&header, sizeof(header)) == 0)
    {
        cout << "服务器: 收到第四次挥手消息" << endl;
        header.seq = 0;
        header.flag = FINAL_CHECK;
        header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
        memcpy(sendBuf, &header, sizeof(header));
        sendto(server, sendBuf, sizeof(header), 0, (sockaddr*)&clientAddr, addrlen);
        cout << "成功发送确认报文" << endl;
        cout << "断开连接" << endl;
    }
    // 如果第四次数据包有问题 那么将等待超时之后重发FIN
    else
    {
        cout << "第四次挥手接收数据有问题" << endl;
        //cout << header.flag << endl;
        //cout << cal_ck_sum((u_short*)&header, sizeof(header));
        while (recvfrom(server, recvBuf, sizeof(header), 0, (sockaddr*)&clientAddr, &addrlen) <= 0)
        {
            if (clock() - start > 0.5 * MAX_TIME)
            {
                cout << "即将重发第三次挥手" << endl;
                goto WAVE_3;
            }
        }


    }
    // 清理资源
    delete[] recvBuf;
    delete[] sendBuf;

    return 0;
}
void Initial() {
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    //指定服务端的性质
    server_addr.sin_family = AF_INET;//使用IPV4
    server_addr.sin_port = htons(8000);//server的端口号
    server_addr.sin_addr.s_addr = htonl(2130706433);//主机127.0.0.1

    //指定路由器的性质
    router_addr.sin_family = AF_INET;//使用IPV4
    router_addr.sin_port = htons(8001);//router的端口号
    router_addr.sin_addr.s_addr = htonl(2130706433);//主机127.0.0.1

    //指定一个客户端
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8002);
    client_addr.sin_addr.s_addr = htonl(2130706433);//主机127.0.0.1

    //绑定服务端
    server = socket(AF_INET, SOCK_DGRAM, 0);
    bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));

    //计算地址性质
    int clen = sizeof(client_addr);
    int rlen = sizeof(router_addr);
    cout << "初始化工作完成" << endl;
}
void recv_file() {
   
    int expectedseqnum = 1;
    HeadMsg header;

    char* recv_buffer = new char[MAX_DATA_LENGTH+ sizeof(header)];
    char* send_buffer = new char[sizeof(header)];
    memset(recv_buffer, 0, MAX_DATA_LENGTH + sizeof(header));
    memset(send_buffer, 0, sizeof(header));

    header.ack = ACK;
    header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
    ioctlsocket(server, FIONBIO, &unblockmode);
    clock_t start=clock();
    while (true) {
        int result;
        while ((result = recvfrom(server, recv_buffer, MAX_DATA_LENGTH + sizeof(header), 0, (sockaddr*)&router_addr, &rlen)) <= 0) {

            if (file_finish == true && clock() - start > MAX_WAIT_TIME) {
                cout << "长时间没有接收到报文，断开连接";
                delete[]send_buffer;
                delete[]recv_buffer;
                ioctlsocket(server, FIONBIO, &blockmode);
                return;
            }
        }
        // 得到数据头
        memcpy(&header, recv_buffer, sizeof(header));
        cout << "接收到长度为" << result << "字节的数据报,头部为: ";
        cout << "seq: " << header.seq << " ack: " << header.ack << " flag: " << header.flag << " checksum: " << header.checkSum << " length: " << header.len << endl;
        u_short chksum = cal_ck_sum((u_short*)recv_buffer, result);

        if (chksum != 0) {
            int n = sendto(server, send_buffer, sizeof(HeadMsg), 0, (sockaddr*)&router_addr, sizeof(SOCKADDR_IN));
            cout << "数据报校验和出错" << endl;
            continue;
        }
        // 接收到一个数据报文
        else if (header.seq == expectedseqnum) {
            // 发回一个ack
            HeadMsg reply_header;
            reply_header.ack = expectedseqnum;
            reply_header.flag = ACK;
            memcpy(send_buffer, (char*)&reply_header, sizeof(reply_header));
            chksum = cal_ck_sum((u_short*)send_buffer, sizeof(reply_header));
            ((HeadMsg*)send_buffer)->checkSum = chksum;
            int n = sendto(server, send_buffer, sizeof(reply_header), 0, (sockaddr*)&client_addr, sizeof(SOCKADDR_IN));
            cout << "发送ACK报文:" << "seq: " << reply_header.seq << " ack: " << reply_header.ack << " flag: " << reply_header.flag << " checksum: " << chksum << " length: " << reply_header.len << endl;
            expectedseqnum++;
            memcpy(message + messagepointer, recv_buffer + sizeof(header), header.len);
            messagepointer += header.len;
            if (header.flag==LAS) {
                file_finish = true;
                start = clock();
                cout << "文件接收完毕" << endl;
                return;
            }
        }
        else {
            sendto(server, send_buffer, sizeof(HeadMsg), 0, (sockaddr*)&client_addr, sizeof(SOCKADDR_IN));
        }
    }
    delete[]send_buffer;
    delete[]recv_buffer;
    ioctlsocket(server, FIONBIO, &blockmode);
}
