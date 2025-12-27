#pragma once
#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <grpcpp/grpcpp.h> // for grpc::Channel, etc
#include <atomic>


using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;


class StatusConPool {
public:
    StatusConPool(size_t  poolSize, std::string host, std::string port):
		b_stop_(false),
		poolSize_(poolSize),
		host_(host),
		port_(port)
	{
		for (size_t i = 0; i < poolSize_; ++i) {
			std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
			connections_.push(StatusService::NewStub(channel));
		}
	}

	~StatusConPool()
	{
		std::lock_guard<std::mutex> lock(mutex_); // 加锁
		Close();
		while(!connections_.empty()) {
			connections_.pop();
		}
	}

	std::unique_ptr<StatusService::Stub> getConnection() {
		std::unique_lock<std::mutex> lock(mutex_); // 加锁
		cond_.wait(lock, [this] { // 等待条件变量
			if (b_stop_) {
				return true;
			}
			return !connections_.empty();
			});
		//如果停止则直接返回空指针
		if (b_stop_) {
			return nullptr;
		}
		auto context = std::move(connections_.front());
		connections_.pop();
		return context; // 返回连接
	}

	void returnConnection(std::unique_ptr<StatusService::Stub> conn) {
		std::lock_guard<std::mutex> lock(mutex_); // 加锁
		if (b_stop_) {
			return;
		}
		connections_.push(std::move(conn)); // 放回连接池
		cond_.notify_one(); // 通知等待线程
	}

	void Close() {
		b_stop_ = true;
		cond_.notify_all(); // 通知所有等待线程
	}
private:
	std::atomic<bool> b_stop_; // 连接池是否停止工作
	size_t poolSize_; // 连接池大小
	std::string host_; // 服务器地址
	std::string  port_; // 服务器端口
	std::queue<std::unique_ptr<StatusService::Stub>> connections_; // 连接池
	std::mutex mutex_; // 互斥锁
	std::condition_variable cond_; // 条件变量

};


class StatusGrpcClient :public Singleton<StatusGrpcClient>
{
    friend class Singleton<StatusGrpcClient>;
public:
    ~StatusGrpcClient() {
    }
    GetChatServerRsp GetChatServer(int uid);
private:
    StatusGrpcClient();
    std::unique_ptr<StatusConPool> pool_;
};