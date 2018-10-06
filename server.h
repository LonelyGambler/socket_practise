#ifndef _SERVER_
#define _SERVER_

#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include <sys/epoll.h>
#include <arpa/inet.h>

using namespace std;

#define SERVER_PORT 40000    //�����������˿�
#define LISTEN_SIZE 1000        //δ��ɶ��еĴ�С
#define MAX_CLI_FD_SET_SIZE (FD_SETSIZE - 1)   //�������ͼ������������
#define PARSE_FAIL -1       
#define PARSE_SUCCESS 0
#define HTTP_REQ_MAX_SIZE 2048      //����HTTP����Ļ�������С
#define HTTP_HERDER_MAX_SIZE 1024   //HTTP��Ӧ��ͷ����С

#define MAX_EP_EV_SIZE 4096            //�����¼����������
#define EPOLL_CREATE_SIZE (MAX_EP_EV_SIZE + 1)  //�ȼ����¼������������
#define EPOLL_WAIT_TIME 5000   //5��

const char g_http_header_ok[] = 
{
	"HTTP/1.1 200 OK\r\n"
	"Content-Type:text/html; charset=utf-8\r\n"
	"Content-Length:%d\r\n"
	"Connection: keep-alive\r\n\r\n"
};

const char g_http_header_err[] = 
{
	"HTTP/1.0 404 Not Found\r\n"
	"Content-Type:text/html; charset=utf-8\r\n"
	"Content-Length:%d\r\n"
	"Connection: keep-alive\r\n\r\n"
};

int parse_data(const char *src, const char *tag_begin, const char *tag_end, char *dest, int dest_len);
void set_nonblocking(int sock);

class block_ser
{
private:
	int m_sock_option;   //�����׽���ѡ��
	int m_ser_sock;
	struct sockaddr_in m_ser_addr;
	
	void init_sock();
public:
	block_ser();
	~block_ser();
	void ser_running();
	static void* pthread_run(void *argv);
};

class select_ser
{
private:
	fd_set m_read_set;
	int *m_sock;         //�׽��־������
	int m_sock_option;   //�����׽���ѡ��
	int m_ser_sock;
	struct sockaddr_in m_ser_addr;
	
	void init_sock();
	void deal_valid_sock();
	void deal_accept_sock();
	void deal_rw_sock(const int idx);
public:
	select_ser();
	~select_ser();	
	void ser_running();
};

class epoll_ser
{
private:
	int m_ep_fd;
	int m_ser_sock;   //�������
	int m_sock_option;   //�����׽���ѡ��
	struct epoll_event m_set_ev;   //���þ���ļ����¼�
	struct epoll_event *m_test_ev;  //������ļ����¼�
	struct sockaddr_in m_ser_addr;
	
	void init_sock(string mode);
	void lt_accept_sock(bool bio_or_nio);
	void et_accept_sock();
	void nonblock_read_sock(int idx);
	void nonblock_write_sock(int idx);
	void block_rw_sock(int idx);
public:
	epoll_ser(string mode);
	~epoll_ser();
	void lt_nonblock_running();
	void lt_block_running();
	void et_nonblock_running();
};

#endif 

