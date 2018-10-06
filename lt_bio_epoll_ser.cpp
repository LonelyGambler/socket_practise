#include "server.h"

using namespace std;

int main(int argc, const char *argv[])
{
	epoll_ser lt_bio_epoll_ser("lt");
	lt_bio_epoll_ser.lt_block_running();
	return 0;
}


