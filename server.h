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

#define SERVER_PORT 40000    //服务器监听端口
#define LISTEN_SIZE 1000        //未完成队列的大小
#define MAX_CLI_FD_SET_SIZE (FD_SETSIZE - 1)   //接入句柄和监听句柄的总数
#define PARSE_FAIL -1       
#define PARSE_SUCCESS 0
#define HTTP_REQ_MAX_SIZE 2048      //接收HTTP请求的缓冲区大小
#define HTTP_HERDER_MAX_SIZE 1024   //HTTP响应的头部大小

#define MAX_EP_EV_SIZE 4096            //监听事件的最大数量
#define EPOLL_CREATE_SIZE (MAX_EP_EV_SIZE + 1)  //比监听事件的最大数量大
#define EPOLL_WAIT_TIME 5000   //5秒

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
	int m_sock_option;   //设置套接字选项
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
	int *m_sock;         //套接字句柄集合
	int m_sock_option;   //设置套接字选项
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
	int m_ser_sock;   //监听句柄
	int m_sock_option;   //设置套接字选项
	struct epoll_event m_set_ev;   //设置句柄的监听事件
	struct epoll_event *m_test_ev;  //检测句柄的监听事件
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

