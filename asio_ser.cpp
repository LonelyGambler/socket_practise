#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include "server.h"
#include <memory>

using namespace boost;

using socket_ptr = std::shared_ptr<asio::ip::tcp::socket>;//包装socket类型的智能指针

void client_session(socket_ptr sock)
{
    auto ep = sock->local_endpoint();
    std::cout<<ep.address().to_string()<<"连接"<<std::endl;

    char data[HTTP_REQ_MAX_SIZE];
    boost::system::error_code ec;
    size_t len;

    while(true)
    {
        len = sock->read_some(asio::buffer(data), ec);
        if(ec)
        {
            std::cout << boost::system::system_error(ec).what() << std::endl;
            break;
        }
		snprintf(data, HTTP_REQ_MAX_SIZE, g_http_header_ok, 0);
        len = sock->write_some(asio::buffer(data), ec);
        if(ec)
        {
            std::cout << boost::system::system_error(ec).what() << std::endl;
            break;
        }
    }
    std::cout<<ep.address().to_string()<<"关闭"<<std::endl;
}

int main(int argc, const char *argv[]) 
{
    asio::io_service service;//创建调度器
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string("0.0.0.0"), 40000);
    asio::ip::tcp::acceptor apt(service, ep);//创建连接器

    while(true)
    {
        socket_ptr sock(new asio::ip::tcp::socket(service));
        apt.accept(*sock);//接收新的连接
        boost::thread(boost::bind(client_session, sock));//开辟线程去处理该连接上的事务
    }

}

