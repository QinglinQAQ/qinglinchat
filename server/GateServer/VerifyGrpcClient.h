#pragma once

#include "message.grpc.pb.h"
#include "const.h"
#include "Singleton.h"


using grpc::Channel; // grpc channel
using grpc::Status; 
using grpc::ClientContext;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

class RPConPool {
public:
	RPConPool(size_t, std::string, std::string);
	~RPConPool();
	std::unique_ptr<VerifyService::Stub> getConnection();
	void returnConnection(std::unique_ptr<VerifyService::Stub>);
	void Close();
private:
	std::atomic<bool> b_stop_; // 连接池是否停止工作
	size_t poolSize_;// 连接池大小
	std::string host_; // 服务器地址
	std::string port_; // 服务器端口
	std::queue<std::unique_ptr<VerifyService::Stub>> connections_; // 连接队列
	std::mutex mutex_; // 互斥锁
	std::condition_variable cond_; // 条件变量
};

class VerifyGrpcClient :public Singleton<VerifyGrpcClient>
{
	friend class Singleton<VerifyGrpcClient>;
public:
	GetVerifyRsp GetVerifyCode(std::string);
private:
	VerifyGrpcClient();
	std::unique_ptr<RPConPool> pool_; // 线程池
};

