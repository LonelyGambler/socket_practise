#include "server.h"

using namespace std;

int main(int argc, const char *argv[])
{
	epoll_ser lt_nio_epoll_ser("lt");
	lt_nio_epoll_ser.lt_nonblock_running();
	return 0;
}



