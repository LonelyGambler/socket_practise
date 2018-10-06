#include "server.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <iostream>
using namespace std;

/*
基于字符串解析，解析返回报文中所需要的字段
[in] src:待解析的字符串
     tag_begin:标签头
     tag_end:标签尾
     dest_len:支持解析的最大长度
[out] dest:解析后得到的字符串
[return] 0成功，其它失败
*/
int parse_data(const char *src, const char *tag_begin, const char *tag_end, char *dest, int dest_len)
{
	assert(src && tag_begin && tag_end && dest);
	char *begin = strstr((char*)src, tag_begin);
	char *end = strstr((char*)src, tag_end);
	int str_begin_len = strlen(tag_begin);
	if (!begin || !end)
	{
		printf("data parse fail,can't find tag_begin(%s) or tag_end(%s).\n", tag_begin, tag_end);
		return PARSE_FAIL;
	}
	begin += str_begin_len;
	int cp_len = end - begin;
	if (cp_len <= 0)
	{
		printf("data parse fail,data error.\n");
		printf("data(%s)\n", src);
		return PARSE_FAIL;
	}
	if (cp_len > dest_len)
	{
		printf("data parse fail,copy length too long.\n");
		return PARSE_FAIL;
	}
	strncpy(dest, begin, cp_len);
	return PARSE_SUCCESS;
}

/*
设置接入句柄为非阻塞状态
[in] sock:套接字句柄
[return] 0表示成功，-1失败
*/
void set_nonblocking(int sock)
{
	int opts;
	opts = fcntl(sock, F_GETFL);
	if(opts < 0)
	{
		perror("fcntl(sock,GETFL)");
		return;
	}
	opts |= O_NONBLOCK;
	if(fcntl(sock, F_SETFL, opts) < 0)
	{
		perror("fcntl(sock,SETFL,opts)");
		return;
	}
	return;
}


block_ser::block_ser()
{
	m_ser_sock = 0;
	memset(&m_ser_addr, 0, sizeof(m_ser_addr));
	init_sock();
}

block_ser::~block_ser()
{
	if (m_ser_sock > 0)
		close(m_ser_sock);
}

/*
初始化服务器相关配置
*/
void block_ser::init_sock()
{
	m_ser_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_ser_sock < 0)
	{
		perror("socket");
		exit(-1);
	}
	//不经过tcp TIME_WAIT,直接让地址端口值重新绑定
	m_sock_option = 1;
	setsockopt(m_ser_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&m_sock_option, sizeof(m_sock_option));
	
	//服务端ip地址与端口值的初始化
	m_ser_addr.sin_family = AF_INET;
	m_ser_addr.sin_port = htons(SERVER_PORT);
	m_ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//绑定
	if (bind(m_ser_sock, (struct sockaddr*)&m_ser_addr, sizeof(m_ser_addr)) < 0)
	{
		perror("bind fail");
		exit(-1);
	}
	//监听
	if (listen(m_ser_sock, LISTEN_SIZE) < 0)
	{
		perror("listen fail");
		exit(-1);
	}
}

/*
服务器进入运行状态
*/
void block_ser::ser_running()
{
	while (1)
	{
		struct sockaddr_in cli_addr;
		int cli_addr_len = sizeof(cli_addr);
		//必须使用堆上的空间，栈上的空间是临时的，有可能出现线程还未创建，但是cli_sock已经改变(接收到了新连接)
		//int cli_sock = accept(m_ser_sock, (struct sockaddr*)&cli_addr, (socklen_t*)&cli_addr_len);
		//printf("ser_running cli_sock addr(%x)\n", &cli_sock);
		int *cli_sock = NULL;
		try
		{
			cli_sock = new int;		
		}
		catch (const bad_alloc& err)
		{
			cout << "unable to request for memory.\n";
			exit(-1);
		}
		*cli_sock = accept(m_ser_sock, (struct sockaddr*)&cli_addr, (socklen_t*)&cli_addr_len);
		if (*cli_sock < 0)
		{
			perror("accept fail");
			exit(-1);
		}
		else 
		{
			//printf("accept(%d)\n", cli_sock);
			pthread_t pth_id;
			int ret = pthread_create(&pth_id, NULL, pthread_run, (void*)cli_sock);
			//int ret = pthread_create(&pth_id, NULL, pthread_run, (void*)&cli_sock);
			while (EOF == ret)
			{
				perror("pthread create fail");
				ret = pthread_create(&pth_id, NULL, pthread_run, (void*)cli_sock);
				//ret = pthread_create(&pth_id, NULL, pthread_run, (void*)&cli_sock);
			}
			pthread_detach(pth_id);
			//pthread_join(pth_id, NULL);
		}
	}
}

/*
线程执行函数
[in] argv: 客户端的连接句柄
*/
void* block_ser::pthread_run(void *argv)
{
	//pthread_detach(pthread_self());
	//printf("pthread_run cli_sock addr(%x)\n", (int*)argv);
	int cli_sock;
	if (argv != NULL)
	{
		cli_sock = *((int*)argv);
		delete (int*)argv;
		argv = NULL;		
	}
	char http_response[HTTP_HERDER_MAX_SIZE] = {0};
	char browser_msg[HTTP_REQ_MAX_SIZE] = {0};
	/*
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	int opt_ret = setsockopt(cli_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	if (opt_ret < 0)
	{
		printf("setsockopt(%d), %s\n", cli_sock, strerror(errno));
	}
	opt_ret = setsockopt(cli_sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
	*/
	int recv_ret, send_ret;
	if ((recv_ret = recv(cli_sock, browser_msg, sizeof(browser_msg)-1, 0)) > 0)
	{
		//printf("recieve data from browser:\n%s\n", browser_msg);
		snprintf(http_response, sizeof(http_response), g_http_header_ok, 0);
		send_ret = send(cli_sock, http_response, strlen(http_response), 0);
		if (send_ret < 0)
		{
			perror("send failed");
		}
			
	}
	else if (recv_ret == 0)
	{
		printf("the client closed.\n");
	}
	else 
	{
		printf("recv (%d), %s\n", cli_sock, strerror(errno));
	}
	close(cli_sock);
	return NULL;
}

select_ser::select_ser()
{
	try
	{
		m_sock = new int[MAX_CLI_FD_SET_SIZE];    //select第一个参数不能超过FD_SETSIZE，监听句柄，连接句柄总和FD_SETSIZE-1
	}
	catch (const bad_alloc& err)
	{
		cout << "unable to request for memory.\n";
		exit(-1);
	}
	memset(m_sock, 0, sizeof(int) * MAX_CLI_FD_SET_SIZE);
	m_ser_sock = 0;
	memset(&m_ser_addr, 0, sizeof(m_ser_addr));
	init_sock();
}

select_ser::~select_ser()
{
	if (m_ser_sock > 0)
		close(m_ser_sock);
	if (m_sock != NULL)
		delete []m_sock;
}

/*
初始化服务器相关配置
*/
void select_ser::init_sock()
{
	m_ser_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_ser_sock < 0)
	{
		perror("socket");
		exit(-1);
	}
	//不经过tcp TIME_WAIT,直接让地址端口值重新绑定
	m_sock_option = 1;
	setsockopt(m_ser_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&m_sock_option, sizeof(m_sock_option));
	
	//服务端ip地址与端口值的初始化
	m_ser_addr.sin_family = AF_INET;
	m_ser_addr.sin_port = htons(SERVER_PORT);
	m_ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//绑定
	if (bind(m_ser_sock, (struct sockaddr*)&m_ser_addr, sizeof(m_ser_addr)) < 0)
	{
		perror("bind fail");
		exit(-1);
	}
	//监听
	if (listen(m_ser_sock, LISTEN_SIZE) < 0)
	{
		perror("listen fail");
		exit(-1);
	}
	m_sock[0] = m_ser_sock;
}

/*
监听句柄监听到有效连接
*/
void select_ser::deal_accept_sock()
{
	int i;
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	unsigned int len = sizeof(client_addr);
	int new_sock = accept(m_ser_sock, (struct sockaddr*)&client_addr, (socklen_t*)&len);
	if (new_sock < 0)
	{
		perror("accept fail");
	}
	else 
	{
		for (i = 1; i < MAX_CLI_FD_SET_SIZE; i++)
		{
			if (m_sock[i] == 0)
			{
				m_sock[i] = new_sock;
				break;
			}
		}
		if (i == MAX_CLI_FD_SET_SIZE)
		{
			printf("too many clients.\n");
		}
		//printf("recv connect ip=%s port=%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	}
	FD_SET(new_sock, &m_read_set);
}

/*
接入句柄有数据可读
*/
void select_ser::deal_rw_sock(const int idx)
{
	char recv_buf[HTTP_REQ_MAX_SIZE] = {0};
	char http_res[HTTP_HERDER_MAX_SIZE] = {0};
	int revc_ret, send_ret;
	if ((revc_ret = recv(m_sock[idx], recv_buf, sizeof(recv_buf), 0)) <= 0)
	{
		if (revc_ret == 0)
			printf("the client closed.\n");
		else
			printf("recv(%d) failed, %s\n", m_sock[idx], strerror(errno));
	}
	//printf("recv data(%s)\n", recv_buf);
	else 
	{
		snprintf(http_res, sizeof(http_res), g_http_header_ok, 0);
		if (send_ret = send(m_sock[idx], http_res, strlen(http_res), 0) < 0)
		{
			printf("send(%d) failed, %s\n", m_sock[idx], strerror(errno));
		}
	}
	FD_CLR(m_sock[idx], &m_read_set);
	close(m_sock[idx]);
	m_sock[idx] = 0;
}

/*
处理有效的套接字
*/
void select_ser::deal_valid_sock()
{
	int i;
	for (i = 0; i < FD_SETSIZE; i++)
	{
		if (FD_ISSET(m_sock[i], &m_read_set))
		{
			if (m_sock[i] == m_ser_sock)
			{
				deal_accept_sock();
			}
			else
			{
				deal_rw_sock(i);
			}
		}
	}
}

/*
服务器进入运行状态
*/
void select_ser::ser_running()
{
	int n_ready;
	while (1)
	{
		FD_ZERO(&m_read_set);
		FD_SET(m_ser_sock, &m_read_set);
		struct timeval tv;
		tv.tv_sec = 5;    //5秒超时时间
		tv.tv_usec = 0;
		n_ready = select(FD_SETSIZE, &m_read_set, NULL, NULL, &tv);
		if (n_ready < 0)
		{
			printf("select ret(%d),%s\n", n_ready, strerror(errno));
		}
		else if (n_ready == 0)
		{
			//printf("time out.\n");
		}
		else 
		{
			deal_valid_sock();
		}
	}
}

epoll_ser::epoll_ser(string mode)
{
	m_ep_fd = -1;    //默认其出错
	m_ser_sock = 0;
	memset(&m_set_ev, 0, sizeof(m_set_ev));
	memset(&m_ser_addr, 0, sizeof(m_ser_addr));
	try
	{
		m_test_ev = new struct epoll_event[MAX_EP_EV_SIZE];		
	}
	catch (const bad_alloc& err)
	{
		cout << "unable to request for memory.\n";
		exit(-1);
	}
	memset(m_test_ev, 0, sizeof(struct epoll_event) * MAX_EP_EV_SIZE);
	init_sock(mode);
}

epoll_ser::~epoll_ser()
{
	if (m_ser_sock > 0)
		close(m_ser_sock);
	if (m_test_ev != NULL)
		delete []m_test_ev;
}

/*
初始化服务器相关配置
*/
void epoll_ser::init_sock(string mode)
{
	m_ep_fd = epoll_create(EPOLL_CREATE_SIZE);   
	m_ser_sock = socket(PF_INET, SOCK_STREAM, 0);
	
	//不经过tcp TIME_WAIT,直接让地址端口值重新绑定
	m_sock_option = 1;
	setsockopt(m_ser_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&m_sock_option, sizeof(m_sock_option));

	//服务端ip地址与端口值的初始化
	m_ser_addr.sin_family = AF_INET;
	m_ser_addr.sin_port = htons(SERVER_PORT);
	m_ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (mode == "et")
		set_nonblocking(m_ser_sock);
	
	//绑定
	if (bind(m_ser_sock, (struct sockaddr*)&m_ser_addr, sizeof(m_ser_addr)) < 0)
	{
		perror("bind fail");
		exit(-1);
	}
	//监听
	if (listen(m_ser_sock, LISTEN_SIZE) < 0)
	{
		perror("listen fail");
		exit(-1);
	}
	
	m_set_ev.data.fd = m_ser_sock;
	if (mode == "et")
		m_set_ev.events = EPOLLIN | EPOLLET;
	else 
		m_set_ev.events = EPOLLIN;
	epoll_ctl(m_ep_fd, EPOLL_CTL_ADD, m_ser_sock, &m_set_ev);
	
}

/*
处理接入句柄，LT模式
[in] bio_or_nio: true是非阻塞读写模式，false是阻塞读写模式
*/
void epoll_ser::lt_accept_sock(bool bio_or_nio)
{
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	unsigned int len = sizeof(client_addr);
	int conn_fd = accept(m_ser_sock, (struct sockaddr*)&client_addr, (socklen_t*)&len);
	if (conn_fd < 0)
	{
		perror("accept fail");
	}
	//printf("recv connect ip=%s port=%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

	if (bio_or_nio)
	{
		set_nonblocking(conn_fd);		
	}
	
	m_set_ev.data.fd = conn_fd;
	m_set_ev.events = EPOLLIN;
	epoll_ctl(m_ep_fd, EPOLL_CTL_ADD, conn_fd, &m_set_ev);
}

/*
处理可读事件，非阻塞
*/
void epoll_ser::nonblock_read_sock(int idx)
{
	int sock = m_test_ev[idx].data.fd;
	if (sock < 0)
		return;
	char recv_data[HTTP_REQ_MAX_SIZE] = {0};
	char *head = recv_data;
	int count = 0;  //已接收计数
	int recv_num = 0;
	bool read_ok = false;
	while (1)
	{
		//确保sock是非阻塞的
		recv_num = recv(sock, head + count, sizeof(recv_data) - 1 - count, 0);
		//printf("recv num(%d)\n", recv_num);
		if (recv_num < 0)
		{
			if(errno == EAGAIN)
			{
				//由于是非阻塞的模式,所以当errno为EAGAIN时,表示当前缓冲区已无数据可读
				//在这里就当作是该次事件已处理处.
				read_ok = true;
				break;
			}
			else if (errno == ECONNRESET)
			{
				//对方发送了RST
				cout << "counterpart send out RST\n";
				goto ERROR;
			}
			else if (errno == EINTR)
			{
				//被信号中断
				continue;
			}
			else
			{
				//其他不可弥补的错误
				printf("errno(%d),unrecovable error(%s)\n", errno, strerror(errno));
				goto ERROR;
			}
		}
		else if (recv_num == 0)
		{
			//这里表示对端的socket已正常关闭.发送过FIN了。			
			cout << "counterpart has shut off in recv\n";
			goto ERROR;
		}
		else if (count >= sizeof(recv_data) - 1)
		{
			//只接收这么多，再多就拒绝
			cout << "too many data from client\n";
			read_ok = true;
			break;
		}
		else
		{
			count += recv_num;
			continue;
		}
	}
	if (read_ok)
	{
		nonblock_write_sock(idx);
	}
	//printf("receive data success.\n");
	return;
ERROR:
	epoll_ctl(m_ep_fd, EPOLL_CTL_DEL, sock, &(m_test_ev[idx]));
	close(sock);
	m_test_ev[idx].data.fd = 0;
	return;
}

/*
处理可写事件，非阻塞
*/
void epoll_ser::nonblock_write_sock(int idx)
{
	int sock = m_test_ev[idx].data.fd;
	int count = 0;
	int send_num = 0;
	char http_res[HTTP_HERDER_MAX_SIZE] = {0};
	snprintf(http_res, sizeof(http_res), g_http_header_ok, 0);
	int send_size = strlen(http_res);
	char *head = http_res;
	while (count < send_size)
	{
		send_num = send(sock, head + count, send_size - count, 0);
		if (send_num < 0)
		{
			if (errno == EAGAIN)
			{
				//对于nonblocking 的socket而言，缓冲区暂时不可写
				break;
			}
			else if(errno == ECONNRESET)
			{
				//对端重置,对方发送了RST
				cout << "counterpart send out RST\n";
				goto END;
			}
			else if (errno == EINTR)
			{
				//被信号中断
				continue;
			}
			else
			{
				//其他错误
				printf("errno(%d),unrecovable error(%s)\n", errno, strerror(errno));
				goto END;
			}
		}
		else if (send_num == 0)
		{
			// 这里表示对端的socket已正常关闭
			//cout << "counterpart has shut off in send\n";
			goto END;
		}
		else
		{
			count += send_num;
		}
	}
END:
	epoll_ctl(m_ep_fd, EPOLL_CTL_DEL, sock, &(m_test_ev[idx]));
	close(sock);
	m_test_ev[idx].data.fd = 0;
	return;
}

/*
处理读写事件，阻塞
*/
void epoll_ser::block_rw_sock(int idx)
{
	int sock = m_test_ev[idx].data.fd;
	char recv_buf[HTTP_REQ_MAX_SIZE] = {0};
	char http_res[HTTP_HERDER_MAX_SIZE] = {0};
	int revc_ret, send_ret;
	if ((revc_ret = recv(sock, recv_buf, sizeof(recv_buf), 0)) <= 0)
	{
		if (revc_ret == 0)
			printf("the client closed.\n");
		else
			printf("recv(%d) failed, %s\n", sock, strerror(errno));
	}
	//printf("recv data(%s)\n", recv_buf);
	else 
	{
		snprintf(http_res, sizeof(http_res), g_http_header_ok, 0);
		if (send_ret = send(sock, http_res, strlen(http_res), 0) < 0)
		{
			printf("send(%d) failed, %s\n", sock, strerror(errno));
		}
	}
	epoll_ctl(m_ep_fd, EPOLL_CTL_DEL, sock, &(m_test_ev[idx]));
	close(sock);
	m_test_ev[idx].data.fd = 0;
	return;
}

/*
进入运行状态，LT模式，非阻塞读写
*/
void epoll_ser::lt_nonblock_running()
{
	int i;
	while (1)
	{
		int ready_fd = epoll_wait(m_ep_fd, m_test_ev, MAX_EP_EV_SIZE, EPOLL_WAIT_TIME);
		for (i = 0; i < ready_fd; i++)
		{
			//如果是已经连接的用户，并且收到数据，那么进行读入。
			if(m_test_ev[i].events & EPOLLIN)
			{
				if (m_test_ev[i].data.fd == m_ser_sock)
				{
					lt_accept_sock(true);
				}
				else 
				{
					nonblock_read_sock(i);
				}
			}
		}
	}
}

/*
进入运行状态，LT模式，阻塞读写
*/
void epoll_ser::lt_block_running()
{
	int i;
	while (1)
	{
		int ready_fd = epoll_wait(m_ep_fd, m_test_ev, MAX_EP_EV_SIZE, EPOLL_WAIT_TIME);
		for (i = 0; i < ready_fd; i++)
		{
			//如果是已经连接的用户，并且收到数据，那么进行读入。
			if(m_test_ev[i].events & EPOLLIN)
			{
				if (m_test_ev[i].data.fd == m_ser_sock)
				{
					lt_accept_sock(false);
				}
				else 
				{
					block_rw_sock(i);
				}
			}
		}
	}
}

void epoll_ser::et_accept_sock()
{
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	unsigned int len = sizeof(client_addr);
	int conn_fd;
	while ((conn_fd = accept(m_ser_sock, (struct sockaddr*)&client_addr, (socklen_t*)&len)) > 0)
	{
		set_nonblocking(conn_fd);
		m_set_ev.data.fd = conn_fd;
		m_set_ev.events = EPOLLIN | EPOLLET;
		epoll_ctl(m_ep_fd, EPOLL_CTL_ADD, conn_fd, &m_set_ev);
	}
	return;
}

void epoll_ser::et_nonblock_running()
{
	int i;
	while (1)
	{
		int ready_fd = epoll_wait(m_ep_fd, m_test_ev, MAX_EP_EV_SIZE, EPOLL_WAIT_TIME);
		for (i = 0; i < ready_fd; i++)
		{
			//如果是已经连接的用户，并且收到数据，那么进行读入。
			if(m_test_ev[i].events & EPOLLIN)
			{
				if (m_test_ev[i].data.fd == m_ser_sock)
				{
					et_accept_sock();
				}
				else 
				{
					nonblock_read_sock(i);
				}
			}
		}
	}
}



