#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <ctime>
#include<mutex>
#include<map>
#include<vector>

#pragma comment(lib, "ws2_32.lib") // socket库
using namespace std;
#define PORT 8000
#define _CRT_SECURE_NO_WARNINGS         // 禁止使用不安全的函数报错
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁止使用旧版本的函数报错
#define IP "127.0.0.1"
#define MAX_TIME 0.5*CLOCKS_PER_SEC
#define TIMEOUT (CLOCKS_PER_SEC / 2)
#define WND 8
const int MAXSIZE = 1024;//传输缓冲区最大长度
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1，FIN = 0
const unsigned char SYN_ACK = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;
const unsigned char FINAL_CHECK = 0x20;//FC=1.FIN=0,OVER=0,FIN=0,ACK=0,SYN=0
const unsigned char OVER = 0x7;//结束标志
const unsigned char LAS = 0x8;
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
bool send_over = false;
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
struct Packet {
    HeadMsg header;
    char Msg[MAX_DATA_LENGTH]={0};
};



u_short base = 1, nextseqnum = 1;
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

class timer {
private:
    mutex mtx;
    bool isrunning;
    u_int rtt;
    clock_t start;
public:
    timer() : rtt(TIMEOUT) {};
    void start_timer(u_int rtt) {
        mtx.lock();
        isrunning = true;
        this->rtt = rtt;
        start = clock();
        mtx.unlock();
    };
    void start_timer() {
        mtx.lock();
        isrunning = true;
        start = clock();
        mtx.unlock();
    }
    void stop_timer() {
        mtx.lock();
        isrunning = false;
        mtx.unlock();
    }
    bool time_out() {
        return isrunning && (clock() - start >= rtt);
    }
    float remain_time() {
        return (rtt - (clock() - start)) / 1000.0 <= 0 ? 0 : (rtt - (clock() - start)) / 1000.0;
    }
};
mutex buffer_lock;
mutex print_lock;
timer my_timer;
struct  packet_timer
{
    Packet* packet;
    timer p_timer;
    bool isacked;
    packet_timer(Packet* packet) : packet(packet), p_timer() {
        p_timer.start_timer();
        isacked = false;
    }
};
vector<packet_timer*> packet_timer_vector;
void Initial();
void send_packet(char* msg, int len);
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
    // 计算校验1
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
        cout << "客户端：收到第三次挥手消息，断开连接" << endl;
        //return 0;
    }
    else {
        cout << "客户端：第三次挥手消息接受失败,即将重新接收" << endl;
        goto WAVE_3;
    }
   
WAVE_4:
    
    header.seq = 4;
    header.flag = ACK;
    header.checkSum = 0;
    header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
    //cout << header.flag << " " << cal_ck_sum((u_short*)&header, sizeof(header)) << endl;
    memcpy(sendBuf, &header, sizeof(header));
    if (sendto(client, sendBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == -1) {
        cout << "客户端：第四次挥手发送失败" << endl;
        return 0;
    }
    else
    {
        cout << "客户端：第四次挥手发送成功" << endl;
         cout << header.flag << " " << cal_ck_sum((u_short*)&header, sizeof(header)) << endl;
        return 0;
    }
    start = clock();
    ioctlsocket(client, FIONBIO, &unlockmode);
    while (recvfrom(client, recvBuf, sizeof(header), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
        if (clock() - start > 0.5 * MAX_TIME) {
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
void receive_thread() {
    // 开启非阻塞模式
    ioctlsocket(client, FIONBIO, &unlockmode);
    char* recv_buffer = new char[sizeof(HeadMsg)];
    HeadMsg* header;

    while (true) {
        if (send_over) {
            ioctlsocket(client, FIONBIO, &lockmode);
            delete[]recv_buffer;
            print_lock.lock();
            cout << "recv线程结束"<<endl;
            print_lock.unlock();
            return;
        }
        while (recvfrom(client, recv_buffer, sizeof(HeadMsg), 0, (sockaddr*)&router_addr, &rlen) <= -1) {
            if (send_over) {
                ioctlsocket(client, FIONBIO, &lockmode);
                delete[]recv_buffer;
                print_lock.lock();
                cout << "recv线程结束" << endl;
                print_lock.unlock();
                return;
            }
       /*     if (my_timer.time_out()) {
                for (auto packet : send_not_check) {
                    sendto(client, (char*)packet, sizeof(HeadMsg) + packet->header.len, 0, (sockaddr*)&router_addr, sizeof(SOCKADDR_IN));
                    print_lock.lock();
                    cout << "超时重传数据包，首部为: seq:" << packet->header.seq << ", ack:" << packet->header.ack << ", flag:" << packet->header.flag << ", checksum:" << packet->header.checkSum << ", len:" << packet->header.len << endl;
                    print_lock.unlock();
                }
                my_timer.start_timer();
            }*/
        }
        header = (HeadMsg*)recv_buffer;
        int chksum = cal_ck_sum((u_short*)recv_buffer, sizeof(HeadMsg));
        if (chksum != 0) {
            continue;
        }
        else if (header->flag == ACK) {
            // 累计确认 一直清缓存清到最新收到的那个ack对应的包
            //int recv_num = header->ack + 1 - base;
            //for (int i = 0; i < recv_num; i++) {
            //    buffer_lock.lock();
            //    if (!send_not_check.empty()) {
            //        delete send_not_check.front();  // 删除第一个元素
            //        send_not_check.erase(send_not_check.begin());
            //    }
            //    buffer_lock.unlock();
            //}
            //base = header->ack + 1;
            int ack_num = header->seq;
            for (auto packet_timer : packet_timer_vector)
            {
                buffer_lock.lock();
                //收到的ack_num是对应包的seq,那么就去找到缓冲区中的包 把他的isacked=true;
                if (packet_timer->packet->header.seq == ack_num)
                {
                    // 停止计时
                    packet_timer->p_timer.stop_timer();
                    packet_timer->isacked = true;

                }
                buffer_lock.unlock();
            }
            // 如果收到的ack是base 更新base 怎么更新 应该是连续更新已经接受了的 
            if (base == ack_num)
            {
                buffer_lock.lock();
                // 对缓冲区从第一个开始遍历 并且从缓冲区中删除 ，连续删除的个数即为base更新的
                int acked = 0;
                while (packet_timer_vector.size())
                {
                    if (packet_timer_vector.front()->isacked)
                    {
                        acked++;
                        delete packet_timer_vector.front();  // 删除第一个元素
                        packet_timer_vector.erase(packet_timer_vector.begin());
                    }
                    else
                    {
                        break;
                    }
                }
                // 更新base
                base = base + acked;
                buffer_lock.unlock();
            }            
            print_lock.lock();
            cout << "接收到来自服务器的数据包，首部为: seq:" << header->seq << ", ack:" << header->ack << ", flag:" << header->flag << ", checksum:" << header->checkSum << ", len:" << header->len << ", 剩余窗口大小:" << WND - (nextseqnum - base) << endl;
            print_lock.unlock();
        }
      /*  if (base != nextseqnum) {
            my_timer.start_timer();
        }
        else {
            my_timer.stop_timer();
        }*/
    }
}
void time_out_thread()
{
    while (true)
    {
        if (send_over)
        {
            print_lock.lock();
            cout << "timeout线程结束" << endl;
            print_lock.unlock();
            return;
        }
        // 玛雅 感觉每一次遍历都很浪费时间呢！
        buffer_lock.lock();
        for (auto packet_timer : packet_timer_vector)
        {
            if (packet_timer->isacked == false && packet_timer->p_timer.time_out())
            {
                    print_lock.lock();
                    cout << "重传";
                    print_lock.unlock();
                    sendto(client, (char*)packet_timer->packet, sizeof(HeadMsg) + packet_timer->packet->header.len, 0, (sockaddr*)&router_addr, sizeof(SOCKADDR_IN));
                    packet_timer->p_timer.start_timer();
                    print_lock.lock();
                    cout << "超时重传数据包，首部为: seq:" << packet_timer->packet->header.seq << ", ack:" << packet_timer->packet->header.ack << ", flag:" << packet_timer->packet->header.flag << ", checksum:" << packet_timer->packet->header.checkSum << ", len:" << packet_timer->packet->header.len << endl;
                    print_lock.unlock();
                
            }
        }
        buffer_lock.unlock();
        if (send_over&&packet_timer_vector.size()==0)
        {
            print_lock.lock();
            cout << "timeout线程结束" << endl;
            print_lock.unlock();
            return;
        }
        Sleep(200);
    }

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
    thread receive_t(receive_thread);
    thread time_out_t(time_out_thread);
    int send_length = 0;
    START_GLOBAL = clock();
    for (; messagepointer < messagelength; messagepointer += MAX_DATA_LENGTH)
    {

        if (messagepointer > messagelength) {
            cout << "我要发完了" << endl;
            send_over = true;
            /*if (endsend() == 1) { return 1; }*/
            //return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH) {
            send_length = MAX_DATA_LENGTH;
        }
        else {
            send_length = messagelength - messagepointer + 1;//需要计算发送的长度
        }
        send_packet(message + messagepointer, send_length);
    }
    //sendmessage();
  
    while (packet_timer_vector.size() != 0)
    {
        continue;
    }
    send_over = true;
    END_GLOBAL = clock();
    receive_t.join();
    time_out_t.join();
    DisconnectClient(client, server_addr, rlen);
    log();
    /*------------------挥手断开---------------------*/
 
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
    int send_length;//数据传输长度
    Packet* packet = new Packet;
    while (true) {

        while (nextseqnum>=base+WND)
        {
            continue;
        }
 
        if (messagepointer > messagelength) {
            cout << "我要发完了" << endl;
            send_over = true;
            /*if (endsend() == 1) { return 1; }*/
            return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH) {
            send_length = MAX_DATA_LENGTH;
        }
        else {
            send_length = messagelength - messagepointer + 1;//需要计算发送的长度
        }
        packet->header.seq = nextseqnum;
        packet->header.len = send_length;
        packet->header.checkSum = 0;
        memcpy(packet->Msg, message + messagepointer, send_length);
        memset(sendbuffer, 0, sizeof(header) + MAX_DATA_LENGTH);
        messagepointer += send_length;
        packet->header.checkSum = cal_ck_sum((u_short*)packet, sizeof(header) + MAX_DATA_LENGTH);
        //buffer_lock.lock();
        memcpy(sendbuffer, packet, sizeof(header) + MAX_DATA_LENGTH);
        //send_not_check.push_back(packet);
        //发送消息
        //buffer_lock.unlock();
        if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
            cout << "数据包发送失败！" << endl;
            return -1;
        }
        else
        {
            print_lock.lock();
            cout << "成功发送数据包，序列号: " << packet->header.seq << " ACK: " << packet->header.ack << " 校验和: " << packet->header.checkSum << endl;
            print_lock.unlock();
        }
        if (base == nextseqnum)
        {
            my_timer.start_timer();
        }
        nextseqnum += 1;

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
//RECV:
//    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
//        if (clock() - start > MAX_TIME) {
//            if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
//                cout << "结束信号发送失败" << endl;
//                return -1;
//            }
//            start = clock();
//            cout << "结束信号反馈超时,即将重传" << endl;
//            goto SEND;
//        }
//    }
//    memcpy(&header, recvbuffer, sizeof(header));
//    if (header.flag == OVER_ACK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
//        cout << "结束消息发送成功，即将完成数据发送" << endl;
//        return 1;
//    }
//    else {
//        cout << "数据包错误，即将重传" << endl;
//        goto RECV;
//    }
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

// 发送单个数据包
void send_packet(char *msg,int len)
{
   
    while (nextseqnum >= base + WND)
    {
        continue;
    }
      
    Packet*packet=new Packet;
    packet->header.seq = nextseqnum;
    packet->header.len = len;
    packet->header.checkSum = 0;
    if (len < MAX_DATA_LENGTH)
    {
        packet->header.flag = LAS;
    }
    memcpy(packet->Msg, msg, len);
    u_short chksum= cal_ck_sum((u_short*)packet, sizeof(packet->header) + len);
    packet->header.checkSum = chksum;
    // 压入缓冲区
    buffer_lock.lock();
    // 将发送的数据包和对应的计时器启动，然后压入我们的vector当中
    packet_timer *pt=new packet_timer(packet);
    packet_timer_vector.push_back(pt);
    //packet_timer_vector.push_back({ &packet, temp_timer });
    //发送消息
    buffer_lock.unlock();
    sendto(client, (char*)packet, (sizeof(HeadMsg) + len), 0, (sockaddr*)&router_addr, rlen);
    print_lock.lock();
    cout << "向服务器发送数据包";
    cout << " seq:" << packet->header.seq << " ack: " << packet->header.ack << " type: " << packet->header.flag <<" checksum: " << packet->header.checkSum << endl;
    
    print_lock.unlock();
        nextseqnum +=1;
}