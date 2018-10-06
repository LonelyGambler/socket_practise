all:bio_ser bio_select_ser lt_bio_epoll_ser lt_nio_epoll_ser et_nio_epoll_ser asio_ser

lib=-lpthread

bio_ser_obj=bio_ser.o server.o
bio_ser:$(bio_ser_obj)
	g++ $(lib) -o bio_ser $(bio_ser_obj)

bio_select_ser_obj=server.o bio_select_ser.o
bio_select_ser:$(bio_select_ser_obj)
	g++ $(lib) -o bio_select_ser $(bio_select_ser_obj)

lt_bio_epoll_ser_obj=server.o lt_bio_epoll_ser.o
lt_bio_epoll_ser:$(lt_bio_epoll_ser_obj)
	g++ $(lib) -o lt_bio_epoll_ser $(lt_bio_epoll_ser_obj)

lt_nio_epoll_ser_obj=server.o lt_nio_epoll_ser.o
lt_nio_epoll_ser:$(lt_nio_epoll_ser_obj)
	g++ $(lib) -o lt_nio_epoll_ser $(lt_nio_epoll_ser_obj)

et_nio_epoll_ser_obj=server.o et_nio_epoll_ser.o
et_nio_epoll_ser:$(et_nio_epoll_ser_obj)
	g++ $(lib) -o et_nio_epoll_ser $(et_nio_epoll_ser_obj)

asio_ser_obj=server.o asio_ser.o
asio_ser:$(asio_ser_obj)
	g++ $(lib) -o asio_ser $(asio_ser_obj)

server.o:server.h
bio_ser.o:server.h
bio_select_ser.o:server.h
lt_bio_epoll_ser.o:server.h
lt_nio_epoll_ser.o:server.h
et_nio_epoll_ser.o:server.h
asio_ser.o:server.h

.PHONY:clean
clean:
	rm -rf *.o	

