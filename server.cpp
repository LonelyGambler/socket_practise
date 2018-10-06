#include "server.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <iostream>
using namespace std;

/*
�����ַ����������������ر���������Ҫ���ֶ�
[in] src:���������ַ���
     tag_begin:��ǩͷ
     tag_end:��ǩβ
     dest_len:֧�ֽ�������󳤶�
[out] dest:������õ����ַ���
[return] 0�ɹ�������ʧ��
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
���ý�����Ϊ������״̬
[in] sock:�׽��־��
[return] 0��ʾ�ɹ���-1ʧ��
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
��ʼ���������������
*/
void block_ser::init_sock()
{
	m_ser_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_ser_sock < 0)
	{
		perror("socket");
		exit(-1);
	}
	//������tcp TIME_WAIT,ֱ���õ�ַ�˿�ֵ���°�
	m_sock_option = 1;
	setsockopt(m_ser_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&m_sock_option, sizeof(m_sock_option));
	
	//�����ip��ַ��˿�ֵ�ĳ�ʼ��
	m_ser_addr.sin_family = AF_INET;
	m_ser_addr.sin_port = htons(SERVER_PORT);
	m_ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//��
	if (bind(m_ser_sock, (struct sockaddr*)&m_ser_addr, sizeof(m_ser_addr)) < 0)
	{
		perror("bind fail");
		exit(-1);
	}
	//����
	if (listen(m_ser_sock, LISTEN_SIZE) < 0)
	{
		perror("listen fail");
		exit(-1);
	}
}

/*
��������������״̬
*/
void block_ser::ser_running()
{
	while (1)
	{
		struct sockaddr_in cli_addr;
		int cli_addr_len = sizeof(cli_addr);
		//����ʹ�ö��ϵĿռ䣬ջ�ϵĿռ�����ʱ�ģ��п��ܳ����̻߳�δ����������cli_sock�Ѿ��ı�(���յ���������)
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
�߳�ִ�к���
[in] argv: �ͻ��˵����Ӿ��
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
		m_sock = new int[MAX_CLI_FD_SET_SIZE];    //select��һ���������ܳ���FD_SETSIZE��������������Ӿ���ܺ�FD_SETSIZE-1
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
��ʼ���������������
*/
void select_ser::init_sock()
{
	m_ser_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_ser_sock < 0)
	{
		perror("socket");
		exit(-1);
	}
	//������tcp TIME_WAIT,ֱ���õ�ַ�˿�ֵ���°�
	m_sock_option = 1;
	setsockopt(m_ser_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&m_sock_option, sizeof(m_sock_option));
	
	//�����ip��ַ��˿�ֵ�ĳ�ʼ��
	m_ser_addr.sin_family = AF_INET;
	m_ser_addr.sin_port = htons(SERVER_PORT);
	m_ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//��
	if (bind(m_ser_sock, (struct sockaddr*)&m_ser_addr, sizeof(m_ser_addr)) < 0)
	{
		perror("bind fail");
		exit(-1);
	}
	//����
	if (listen(m_ser_sock, LISTEN_SIZE) < 0)
	{
		perror("listen fail");
		exit(-1);
	}
	m_sock[0] = m_ser_sock;
}

/*
���������������Ч����
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
�����������ݿɶ�
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
������Ч���׽���
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
��������������״̬
*/
void select_ser::ser_running()
{
	int n_ready;
	while (1)
	{
		FD_ZERO(&m_read_set);
		FD_SET(m_ser_sock, &m_read_set);
		struct timeval tv;
		tv.tv_sec = 5;    //5�볬ʱʱ��
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
	m_ep_fd = -1;    //Ĭ�������
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
��ʼ���������������
*/
void epoll_ser::init_sock(string mode)
{
	m_ep_fd = epoll_create(EPOLL_CREATE_SIZE);   
	m_ser_sock = socket(PF_INET, SOCK_STREAM, 0);
	
	//������tcp TIME_WAIT,ֱ���õ�ַ�˿�ֵ���°�
	m_sock_option = 1;
	setsockopt(m_ser_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&m_sock_option, sizeof(m_sock_option));

	//�����ip��ַ��˿�ֵ�ĳ�ʼ��
	m_ser_addr.sin_family = AF_INET;
	m_ser_addr.sin_port = htons(SERVER_PORT);
	m_ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (mode == "et")
		set_nonblocking(m_ser_sock);
	
	//��
	if (bind(m_ser_sock, (struct sockaddr*)&m_ser_addr, sizeof(m_ser_addr)) < 0)
	{
		perror("bind fail");
		exit(-1);
	}
	//����
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
�����������LTģʽ
[in] bio_or_nio: true�Ƿ�������дģʽ��false��������дģʽ
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
����ɶ��¼���������
*/
void epoll_ser::nonblock_read_sock(int idx)
{
	int sock = m_test_ev[idx].data.fd;
	if (sock < 0)
		return;
	char recv_data[HTTP_REQ_MAX_SIZE] = {0};
	char *head = recv_data;
	int count = 0;  //�ѽ��ռ���
	int recv_num = 0;
	bool read_ok = false;
	while (1)
	{
		//ȷ��sock�Ƿ�������
		recv_num = recv(sock, head + count, sizeof(recv_data) - 1 - count, 0);
		//printf("recv num(%d)\n", recv_num);
		if (recv_num < 0)
		{
			if(errno == EAGAIN)
			{
				//�����Ƿ�������ģʽ,���Ե�errnoΪEAGAINʱ,��ʾ��ǰ�������������ݿɶ�
				//������͵����Ǹô��¼��Ѵ���.
				read_ok = true;
				break;
			}
			else if (errno == ECONNRESET)
			{
				//�Է�������RST
				cout << "counterpart send out RST\n";
				goto ERROR;
			}
			else if (errno == EINTR)
			{
				//���ź��ж�
				continue;
			}
			else
			{
				//���������ֲ��Ĵ���
				printf("errno(%d),unrecovable error(%s)\n", errno, strerror(errno));
				goto ERROR;
			}
		}
		else if (recv_num == 0)
		{
			//�����ʾ�Զ˵�socket�������ر�.���͹�FIN�ˡ�			
			cout << "counterpart has shut off in recv\n";
			goto ERROR;
		}
		else if (count >= sizeof(recv_data) - 1)
		{
			//ֻ������ô�࣬�ٶ�;ܾ�
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
�����д�¼���������
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
				//����nonblocking ��socket���ԣ���������ʱ����д
				break;
			}
			else if(errno == ECONNRESET)
			{
				//�Զ�����,�Է�������RST
				cout << "counterpart send out RST\n";
				goto END;
			}
			else if (errno == EINTR)
			{
				//���ź��ж�
				continue;
			}
			else
			{
				//��������
				printf("errno(%d),unrecovable error(%s)\n", errno, strerror(errno));
				goto END;
			}
		}
		else if (send_num == 0)
		{
			// �����ʾ�Զ˵�socket�������ر�
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
�����д�¼�������
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
��������״̬��LTģʽ����������д
*/
void epoll_ser::lt_nonblock_running()
{
	int i;
	while (1)
	{
		int ready_fd = epoll_wait(m_ep_fd, m_test_ev, MAX_EP_EV_SIZE, EPOLL_WAIT_TIME);
		for (i = 0; i < ready_fd; i++)
		{
			//������Ѿ����ӵ��û��������յ����ݣ���ô���ж��롣
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
��������״̬��LTģʽ��������д
*/
void epoll_ser::lt_block_running()
{
	int i;
	while (1)
	{
		int ready_fd = epoll_wait(m_ep_fd, m_test_ev, MAX_EP_EV_SIZE, EPOLL_WAIT_TIME);
		for (i = 0; i < ready_fd; i++)
		{
			//������Ѿ����ӵ��û��������յ����ݣ���ô���ж��롣
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
			//������Ѿ����ӵ��û��������յ����ݣ���ô���ж��롣
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



