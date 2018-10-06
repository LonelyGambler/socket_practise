#include "server.h"

using namespace std;

int main(int argc, const char *argv[])
{
	epoll_ser et_nio_epoll_ser("et");
	et_nio_epoll_ser.et_nonblock_running();
	return 0;
}

