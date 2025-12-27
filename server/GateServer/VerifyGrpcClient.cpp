#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"

VerifyGrpcClient::VerifyGrpcClient(){
	auto& gCfgMgr = ConfigMgr::Inst();
	std::string host = gCfgMgr["VerifyServer"]["Host"];
	std::string port = gCfgMgr["VerifyServer"]["Port"];
	std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
	pool_.reset(new RPConPool(5, host, port)); // 创建连接池，大小为5
}

GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email) {
	ClientContext context;
	GetVerifyRsp reply;
	GetVerifyReq request;
	request.set_email(email);
	auto stub = pool_->getConnection(); // 从连接池获取连接
	Status status = stub->GetVerifyCode(&context, request, &reply);

	if (status.ok()) {
		pool_->returnConnection(std::move(stub)); // 归还连接
		return reply;
	}
	else {
		pool_->returnConnection(std::move(stub)); // 归还连接
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}

RPConPool::RPConPool(size_t poolSize, std::string host, std::string port)
: poolSize_(poolSize), host_(host), port_(port), b_stop_(false) {
	for (size_t i = 0; i < poolSize_; ++i) {

		std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port,
			grpc::InsecureChannelCredentials());

		connections_.push(VerifyService::NewStub(channel));
	}
}

RPConPool::~RPConPool() {
	std::lock_guard<std::mutex> lock(mutex_);
	Close(); // 
	while (!connections_.empty()) {
		connections_.pop();
	}
}

std::unique_ptr<VerifyService::Stub> RPConPool::getConnection() {
	std::unique_lock<std::mutex> lock(mutex_); // 加锁
	cond_.wait(lock, [this] { // 等待条件变量
		if (b_stop_) {
			return true;
		}
		return !connections_.empty();
		});
	//如果停止则直接返回空指针
	if (b_stop_) {
		return  nullptr;
	}
	auto context = std::move(connections_.front());
	connections_.pop();
	return context;
}

void RPConPool::returnConnection(std::unique_ptr<VerifyService::Stub> context) {
	std::lock_guard<std::mutex> lock(mutex_); // 加锁
	if (b_stop_) {
		return;
	}
	connections_.push(std::move(context)); // 归还连接
	cond_.notify_one(); // 通知等待的线程
}

void RPConPool::Close() {
	b_stop_ = true;
	cond_.notify_all();
}