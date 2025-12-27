#include "CServer.h"
#include "HttpConnection.h"
#include "AsioIOServicePool.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port)
	: _ioc(ioc),
	_acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
	_socket(ioc)
{

}

void CServer::StartAccept()
{
	auto self = shared_from_this(); // 为了在异步操作中保持对象存活
	auto& io_context = AsioIOServicePool::GetInstance()->GetIOService(); // 获取io_context
	//新连接
	std::shared_ptr<HttpConnection> new_connection = std::make_shared<HttpConnection>(io_context); 
	_acceptor.async_accept(new_connection->GetSocket(),
		[self, new_connection](beast::error_code ec) { // 接受连接的回调
			try {
				//出错放弃链接，继续接受下一个链接
				if (ec) {
					self->StartAccept();
					return;
				}
				// 连接成功，启动连接处理
				new_connection->Start();
				self->StartAccept(); // 继续接受下一个连接
			}
			catch (std::exception& e) {
				std::cout << "accept exception is " << e.what() << std::endl;
				self->StartAccept();
			}
		}
	);
}
