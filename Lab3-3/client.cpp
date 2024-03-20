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

#pragma comment(lib, "ws2_32.lib") // socket��
using namespace std;
#define PORT 8000
#define _CRT_SECURE_NO_WARNINGS         // ��ֹʹ�ò���ȫ�ĺ�������
#define _WINSOCK_DEPRECATED_NO_WARNINGS // ��ֹʹ�þɰ汾�ĺ�������
#define IP "127.0.0.1"
#define MAX_TIME 0.5*CLOCKS_PER_SEC
#define TIMEOUT (CLOCKS_PER_SEC / 2)
#define WND 8
const int MAXSIZE = 1024;//���仺������󳤶�
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1��FIN = 0
const unsigned char SYN_ACK = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;
const unsigned char FINAL_CHECK = 0x20;//FC=1.FIN=0,OVER=0,FIN=0,ACK=0,SYN=0
const unsigned char OVER = 0x7;//������־
const unsigned char LAS = 0x8;
const unsigned char OVER_ACK = 0xA;
#define MAX_DATA_LENGTH 4096 // ����������ݳ���Ϊ512�ֽڣ�����ʵ���������
clock_t connectClock;
clock_t start;
clock_t END_GLOBAL;
clock_t START_GLOBAL;
u_long unlockmode = 1;
u_long lockmode = 0;
char* message = new char[100000000];
unsigned long long int messagelength = 0;//���һ��Ҫ�����±��Ƕ���
unsigned long long int messagepointer = 0;//��һ���ô���λ��
WSADATA wsadata;
//�����׽�����Ҫ�󶨵ĵ�ַ
SOCKADDR_IN server_addr;
//����·������Ҫ�󶨵ĵ�ַ
SOCKADDR_IN router_addr;
//�����ͻ��˵ĵ�ַ
SOCKADDR_IN client_addr;
SOCKET client;
int slen = sizeof(server_addr);
int rlen = sizeof(router_addr);
bool send_over = false;
struct  HeadMsg
{
    u_short len;			// ���ݳ���16λ
    u_short checkSum;		// 16λУ���
    u_short flag; // FIN ACK SYN  
    u_short ack;
    u_short seq;		// ���кţ����Ա�ʾ0-255

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
    // 1. ���͵�һ������
    //cout << "׼�����͵�һ������" << endl;
    char* sendBuf = new char[sizeof(header) + 1];
    char* recvBuf = new char[sizeof(header) + 1];
    memset(sendBuf, 0, sizeof(sendBuf));

    // ����ͷ����Ϣ
    header.flag = SYN;
    header.seq = 0;
    header.len = 0;
    header.checkSum = 0;
    u_short sum = cal_ck_sum((u_short*)&header, sizeof(header));
    header.checkSum = sum;
    // ��װ��Ϣ
    memcpy(sendBuf, &header, sizeof(header));
FIRST_SHAKE:
    // ���͵�һ������
    if (sendto(client, sendBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, serverAddrlen) == -1) {
        cout << "�ͻ��ˣ���һ����������ʧ�ܣ�" << endl;
        return false;
    }
    else {
        cout << "�ͻ��ˣ���һ���������ӳɹ���" << endl;
    }
    // ����Ϊ������ģʽ
    u_long mode = 1;
    ioctlsocket(client, FIONBIO, &mode);
    // �趨��ʱ��
    connectClock = clock();
    // 2. �ȴ���������Ӧ�ڶ�������
    while (true) {
        if (recvfrom(client, recvBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, &serverAddrlen) > 0) {
            memcpy(&header, recvBuf, sizeof(header));
            if (header.flag == SYN_ACK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
                cout << "�ͻ���: �ڶ����������ӳɹ���" << endl;
                break;
            }
            else {
                return false;
            }
        }
        if (clock() - connectClock > 75 * CLOCKS_PER_SEC)
        {
            if (sendto(client, sendBuf, sizeof(header), 0, (sockaddr*)&serverAddr, serverAddrlen) == -1) {
                cout << "�ͻ��ˣ���һ������������ʧ��..." << endl;
                return false;
            }
            start = clock();
            cout << "�ͻ��ˣ���һ��������Ϣ������ʱ,�������·���" << endl;
            goto FIRST_SHAKE;
        }
    }

    // 3. ���͵���������
    header.flag = ACK;
    header.checkSum = 0;
    header.seq = 1;
    sum = cal_ck_sum((u_short*)&header, sizeof(header));
    header.checkSum = sum;

    // ��װ��Ϣ
    memcpy(sendBuf, &header, sizeof(header));

    // ���͵���������
THIRD_SHAKE:
    if (sendto(client, sendBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == -1) {

        cout << "�ͻ��ˣ����������ַ���ʧ��" << endl;
        return false;
    }
    else
    {
        cout << "�ͻ��ˣ����������ַ��ͳɹ�" << endl;
    }


    return true;
}
void log();
int DisconnectClient(SOCKET& client, SOCKADDR_IN& serverAddr, int& addrlen)
{
    HeadMsg header;
    char* sendBuf = new char[sizeof(header) + 1];
    char* recvBuf = new char[sizeof(header) + 1];

    // ��һ�λ��֣��ͻ��˷��ͶϿ���������
    header.flag = FIN;         // FIN
    header.checkSum = 0;
    header.seq = 0;          // �������к�
    // ����У��1
    header.checkSum = cal_ck_sum((u_short*)&header, sizeof(header));
    // ��װ��Ϣ
    memcpy(sendBuf, &header, sizeof(header));
WAVE_1:
    // ���͵�һ�λ���
    if (sendto(client, sendBuf, sizeof(header), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == -1) {
        return 0;
    }
    else
    {
        cout << "�ͻ��ˣ���һ�λ��ַ��ͳɹ�" << endl;
    }
WAVE_2:
    u_long mode = 0;  // 0 ��ʾ����ģʽ��1 ��ʾ������ģʽ
    ioctlsocket(client, FIONBIO, &mode);
    // �ڶ��λ��֣��ȴ���������Ӧ
    clock_t start = clock();
    while (recvfrom(client, recvBuf, sizeof(header), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {

    }
    if (clock() - start > 75 * MAX_TIME) {

        if (sendto(client, sendBuf, sizeof(header), 0, (sockaddr*)&serverAddr, addrlen) == -1) {
            cout << "�ͻ��ˣ���һ�λ��ַ���ʧ��" << endl;
            return -1;
        }
        cout << "�ͻ��ˣ���һ�λ��ַ������ճ�ʱ���ط���һ�λ���" << endl;
        start = clock();
        goto WAVE_1;

    }
    memcpy(&header, recvBuf, sizeof(header));
    if (header.flag == ACK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
        //cout << cal_ck_sum((u_short*)&header, sizeof(header)) << endl;
        cout << "�ͻ��ˣ��յ��ڶ��λ�����Ϣ" << endl;
    }
    else {
        cout << "�ͻ��ˣ��ڶ��λ�����Ϣ����ʧ��...." << endl;
        goto WAVE_2;
    }
    // ������ ���ֵȴ���������FIN
WAVE_3:
    while (recvfrom(client, recvBuf, sizeof(header), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {}
    memcpy(&header, recvBuf, sizeof(header));
    if (header.flag == FIN && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
        cout << "�ͻ��ˣ��յ������λ�����Ϣ���Ͽ�����" << endl;
        //return 0;
    }
    else {
        cout << "�ͻ��ˣ������λ�����Ϣ����ʧ��,�������½���" << endl;
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
        cout << "�ͻ��ˣ����Ĵλ��ַ���ʧ��" << endl;
        return 0;
    }
    else
    {
        cout << "�ͻ��ˣ����Ĵλ��ַ��ͳɹ�" << endl;
         cout << header.flag << " " << cal_ck_sum((u_short*)&header, sizeof(header)) << endl;
        return 0;
    }
    start = clock();
    ioctlsocket(client, FIONBIO, &unlockmode);
    while (recvfrom(client, recvBuf, sizeof(header), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
        if (clock() - start > 0.5 * MAX_TIME) {
            cout << "�ͻ��ˣ����ܷ�����ʱ���ط����Ĵλ���" << endl;
            goto WAVE_4;
        }
    }
    memcpy(&header, recvBuf, sizeof(header));
    if (header.flag == FINAL_CHECK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
        cout << "�ͻ��ˣ��Ĵλ������,�����Ͽ�����" << endl;
        return true;
    }
    else {
        cout << "�ͻ��ˣ����ݰ�����,׼���ط����Ĵλ���" << endl;
        goto WAVE_4;
    }

    // ������Դ
    delete[] recvBuf;
    delete[] sendBuf;

    return true;
}
void receive_thread() {
    // ����������ģʽ
    ioctlsocket(client, FIONBIO, &unlockmode);
    char* recv_buffer = new char[sizeof(HeadMsg)];
    HeadMsg* header;

    while (true) {
        if (send_over) {
            ioctlsocket(client, FIONBIO, &lockmode);
            delete[]recv_buffer;
            print_lock.lock();
            cout << "recv�߳̽���"<<endl;
            print_lock.unlock();
            return;
        }
        while (recvfrom(client, recv_buffer, sizeof(HeadMsg), 0, (sockaddr*)&router_addr, &rlen) <= -1) {
            if (send_over) {
                ioctlsocket(client, FIONBIO, &lockmode);
                delete[]recv_buffer;
                print_lock.lock();
                cout << "recv�߳̽���" << endl;
                print_lock.unlock();
                return;
            }
       /*     if (my_timer.time_out()) {
                for (auto packet : send_not_check) {
                    sendto(client, (char*)packet, sizeof(HeadMsg) + packet->header.len, 0, (sockaddr*)&router_addr, sizeof(SOCKADDR_IN));
                    print_lock.lock();
                    cout << "��ʱ�ش����ݰ����ײ�Ϊ: seq:" << packet->header.seq << ", ack:" << packet->header.ack << ", flag:" << packet->header.flag << ", checksum:" << packet->header.checkSum << ", len:" << packet->header.len << endl;
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
            // �ۼ�ȷ�� һֱ�建���嵽�����յ����Ǹ�ack��Ӧ�İ�
            //int recv_num = header->ack + 1 - base;
            //for (int i = 0; i < recv_num; i++) {
            //    buffer_lock.lock();
            //    if (!send_not_check.empty()) {
            //        delete send_not_check.front();  // ɾ����һ��Ԫ��
            //        send_not_check.erase(send_not_check.begin());
            //    }
            //    buffer_lock.unlock();
            //}
            //base = header->ack + 1;
            int ack_num = header->seq;
            for (auto packet_timer : packet_timer_vector)
            {
                buffer_lock.lock();
                //�յ���ack_num�Ƕ�Ӧ����seq,��ô��ȥ�ҵ��������еİ� ������isacked=true;
                if (packet_timer->packet->header.seq == ack_num)
                {
                    // ֹͣ��ʱ
                    packet_timer->p_timer.stop_timer();
                    packet_timer->isacked = true;

                }
                buffer_lock.unlock();
            }
            // ����յ���ack��base ����base ��ô���� Ӧ�������������Ѿ������˵� 
            if (base == ack_num)
            {
                buffer_lock.lock();
                // �Ի������ӵ�һ����ʼ���� ���Ҵӻ�������ɾ�� ������ɾ���ĸ�����Ϊbase���µ�
                int acked = 0;
                while (packet_timer_vector.size())
                {
                    if (packet_timer_vector.front()->isacked)
                    {
                        acked++;
                        delete packet_timer_vector.front();  // ɾ����һ��Ԫ��
                        packet_timer_vector.erase(packet_timer_vector.begin());
                    }
                    else
                    {
                        break;
                    }
                }
                // ����base
                base = base + acked;
                buffer_lock.unlock();
            }            
            print_lock.lock();
            cout << "���յ����Է����������ݰ����ײ�Ϊ: seq:" << header->seq << ", ack:" << header->ack << ", flag:" << header->flag << ", checksum:" << header->checkSum << ", len:" << header->len << ", ʣ�ര�ڴ�С:" << WND - (nextseqnum - base) << endl;
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
            cout << "timeout�߳̽���" << endl;
            print_lock.unlock();
            return;
        }
        // ���� �о�ÿһ�α��������˷�ʱ���أ�
        buffer_lock.lock();
        for (auto packet_timer : packet_timer_vector)
        {
            if (packet_timer->isacked == false && packet_timer->p_timer.time_out())
            {
                    print_lock.lock();
                    cout << "�ش�";
                    print_lock.unlock();
                    sendto(client, (char*)packet_timer->packet, sizeof(HeadMsg) + packet_timer->packet->header.len, 0, (sockaddr*)&router_addr, sizeof(SOCKADDR_IN));
                    packet_timer->p_timer.start_timer();
                    print_lock.lock();
                    cout << "��ʱ�ش����ݰ����ײ�Ϊ: seq:" << packet_timer->packet->header.seq << ", ack:" << packet_timer->packet->header.ack << ", flag:" << packet_timer->packet->header.flag << ", checksum:" << packet_timer->packet->header.checkSum << ", len:" << packet_timer->packet->header.len << endl;
                    print_lock.unlock();
                
            }
        }
        buffer_lock.unlock();
        if (send_over&&packet_timer_vector.size()==0)
        {
            print_lock.lock();
            cout << "timeout�߳̽���" << endl;
            print_lock.unlock();
            return;
        }
        Sleep(200);
    }

}

int main()
{

    /*------------------���Ӳ���---------------------*/
    Initial();
    ConnectClient(client, server_addr, slen);
    /*------------------������Ϣ---------------------*/
     //�Զ����Ʒ�ʽ���ļ�
    string filename;
    cout << "������Ҫ������ļ���" << endl;
    cin >> filename;
    ifstream fin(filename.c_str(), ifstream::binary);//�Զ����Ʒ�ʽ���ļ�
    unsigned long long int index = 0;
    unsigned char temp = fin.get();
    while (fin)
    {
        message[index++] = temp;
        temp = fin.get();
    }
    messagelength = index - 1;
    fin.close();
    cout << "����ļ����빤��" << endl;
    thread receive_t(receive_thread);
    thread time_out_t(time_out_thread);
    int send_length = 0;
    START_GLOBAL = clock();
    for (; messagepointer < messagelength; messagepointer += MAX_DATA_LENGTH)
    {

        if (messagepointer > messagelength) {
            cout << "��Ҫ������" << endl;
            send_over = true;
            /*if (endsend() == 1) { return 1; }*/
            //return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH) {
            send_length = MAX_DATA_LENGTH;
        }
        else {
            send_length = messagelength - messagepointer + 1;//��Ҫ���㷢�͵ĳ���
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
    /*------------------���ֶϿ�---------------------*/
 
    // �رտͻ����׽���
    closesocket(client);
    // �ͷ� Winsock
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

    //ָ��һ���ͻ���
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8002);
    client_addr.sin_addr.s_addr = htonl(2130706433);

    client = socket(AF_INET, SOCK_DGRAM, 0);
    bind(client, (SOCKADDR*)&client_addr, sizeof(client_addr));
    cout << "��ʼ���" << endl;
}

int sendmessage() {
    START_GLOBAL = clock();
    //�����Ƿ�Ϊ������ģʽ
    ioctlsocket(client, FIONBIO, &unlockmode);
    HeadMsg header;
    char* recvbuffer = new char[sizeof(header)];
    char* sendbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];
    int currentSeq = 0;
    int send_length;//���ݴ��䳤��
    Packet* packet = new Packet;
    while (true) {

        while (nextseqnum>=base+WND)
        {
            continue;
        }
 
        if (messagepointer > messagelength) {
            cout << "��Ҫ������" << endl;
            send_over = true;
            /*if (endsend() == 1) { return 1; }*/
            return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH) {
            send_length = MAX_DATA_LENGTH;
        }
        else {
            send_length = messagelength - messagepointer + 1;//��Ҫ���㷢�͵ĳ���
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
        //������Ϣ
        //buffer_lock.unlock();
        if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
            cout << "���ݰ�����ʧ�ܣ�" << endl;
            return -1;
        }
        else
        {
            print_lock.lock();
            cout << "�ɹ��������ݰ������к�: " << packet->header.seq << " ACK: " << packet->header.ack << " У���: " << packet->header.checkSum << endl;
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
        cout << "�����źŷ���ʧ��" << endl;
        return -1;
    }
    cout << "�����źŷ��ͳɹ�" << endl;
    clock_t start = clock();
//RECV:
//    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
//        if (clock() - start > MAX_TIME) {
//            if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
//                cout << "�����źŷ���ʧ��" << endl;
//                return -1;
//            }
//            start = clock();
//            cout << "�����źŷ�����ʱ,�����ش�" << endl;
//            goto SEND;
//        }
//    }
//    memcpy(&header, recvbuffer, sizeof(header));
//    if (header.flag == OVER_ACK && cal_ck_sum((u_short*)&header, sizeof(header)) == 0) {
//        cout << "������Ϣ���ͳɹ�������������ݷ���" << endl;
//        return 1;
//    }
//    else {
//        cout << "���ݰ����󣬼����ش�" << endl;
//        goto RECV;
//    }
}
void log() {
    cout << "*********������־********" << endl;
    cout << "**************************" << endl;
    cout << "���δ��䱨���ܳ��ȣ�" << messagepointer << "�ֽ�" << endl;
    cout << "���У�" << (messagepointer / 256) + 1 << "�����Ķηֱ�ת��" << endl;

    double transferTime = static_cast<double>(END_GLOBAL - START_GLOBAL) / CLOCKS_PER_SEC;
    cout << "���δ���ʱ�䣺" << transferTime << "��" << endl;

    if (transferTime > 0) {
        double throughput = static_cast<double>(messagepointer) / transferTime;
        cout << "���δ��������ʣ�" << throughput << "�ֽ�/��" << endl;
    }
    else {
        cout << "���δ����������޷����㣨����ʱ��Ϊ�㣩" << endl;
    }
}

// ���͵������ݰ�
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
    // ѹ�뻺����
    buffer_lock.lock();
    // �����͵����ݰ��Ͷ�Ӧ�ļ�ʱ��������Ȼ��ѹ�����ǵ�vector����
    packet_timer *pt=new packet_timer(packet);
    packet_timer_vector.push_back(pt);
    //packet_timer_vector.push_back({ &packet, temp_timer });
    //������Ϣ
    buffer_lock.unlock();
    sendto(client, (char*)packet, (sizeof(HeadMsg) + len), 0, (sockaddr*)&router_addr, rlen);
    print_lock.lock();
    cout << "��������������ݰ�";
    cout << " seq:" << packet->header.seq << " ack: " << packet->header.ack << " type: " << packet->header.flag <<" checksum: " << packet->header.checkSum << endl;
    
    print_lock.unlock();
        nextseqnum +=1;
}