#include "RedisMgr.h"
#include "ConfigMgr.h"

RedisConPool::RedisConPool(size_t poolSize, const char* host, int port, const char* password)
	: poolSize_(poolSize), host_(host), port_(port), password_(password), b_stop_(false) {
	for (size_t i = 0; i < poolSize_; ++i) {
		auto* context = redisConnect(host_.c_str(), port_);
		if (context == nullptr || context->err) {
			if (context) {
				std::cout << "Redis connection error: " << context->errstr << std::endl;
				redisFree(context);
			}
			else {
				std::cout << "Redis connection error: can't allocate redis context" << std::endl;
			}
			continue;
		}
		if (password_ != "") {
			auto* reply = (redisReply*)redisCommand(context, "AUTH %s", password_.c_str());
			if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
				std::cout << "认证失败" << std::endl;
				freeReplyObject(reply);
				redisFree(context);
				continue;
			}
			freeReplyObject(reply);
		}
		//std::cout << "认证成功（pool）" << std::endl;
		connections_.push(context);
	}
}


RedisConPool::~RedisConPool() {

	std::lock_guard<std::mutex> lock(mutex_); // 加锁
	while (!connections_.empty()) {
		connections_.pop();
	}
}

redisContext* RedisConPool::getConnection() {
	std::unique_lock<std::mutex> lock(mutex_); // 加锁
	cond_.wait(lock, [this] { // 等待条件变量
		if (b_stop_) {
			return true;
		}
		return !connections_.empty(); // 如果连接池不为空则返回true
		});
	//如果停止则直接返回空指针
	if (b_stop_) {
		return  nullptr;
	}
	auto* conn = connections_.front();
	connections_.pop();
	return conn;
}

void RedisConPool::returnConnection(redisContext* conn) {
	std::lock_guard<std::mutex> lock(mutex_); // 加锁
	// 如果线程池关闭
	if (b_stop_) {
		return;
	}
	connections_.push(conn);
	cond_.notify_one(); // 通知等待的线程
}

void RedisConPool::Close() {
	b_stop_ = true;
	cond_.notify_all(); // 通知所有等待的线程
}

RedisMgr::RedisMgr() {
	auto& gCfgMgr = ConfigMgr::Inst();
	auto host = gCfgMgr["Redis"]["Host"];
	auto port = gCfgMgr["Redis"]["Port"];
	auto pwd = gCfgMgr["Redis"]["Passwd"];
	con_pool_.reset(new RedisConPool(5, host.c_str(), atoi(port.c_str()), pwd.c_str()));
}

RedisMgr::~RedisMgr() {
	std::cout << "before RedisMgr::Close()" << std::endl;
	Close();
}

// 连接redis服务器
bool RedisMgr::Connect(const std::string& host, int port)
{
	auto connect = con_pool_->getConnection();
	if(connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	if (connect != nullptr && connect->err)
	{
		std::cout << "connect error " << connect->errstr << std::endl;
		con_pool_->returnConnection(connect);
		return false;
	}
	con_pool_->returnConnection(connect);
	return true;
}

// 获取key对应的value
bool RedisMgr::Get(const std::string& key, std::string& value)
{
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "GET %s", key.c_str());
	if (reply == nullptr) {
		std::cout << "[ GET  " << key << " ] failed" << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_STRING) {
		std::cout << "[ GET  " << key << " ] failed" << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}

	value = reply->str;
	freeReplyObject(reply);
	con_pool_->returnConnection(connect);
	std::cout << "Succeed to execute command [ GET " << key << "  ]" << std::endl;
	return true;
}

// 设置key对应的value
bool RedisMgr::Set(const std::string& key, const std::string& value) {
	//执行redis命令行
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}

	auto reply = (redisReply*)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());

	//如果返回NULL则说明执行失败
	if (nullptr == reply)
	{
		std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}

	//如果执行失败则释放连接
	if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
	{
		std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}

	//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
	freeReplyObject(reply);
	std::cout << "Execut command [ SET " << key << "  " << value << " ] success ! " << std::endl;
	return true;
}

// 认证密码
bool RedisMgr::Auth(const std::string& password)
{
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}

	auto reply = (redisReply*)redisCommand(connect, "AUTH %s", password.c_str());
	if (reply ==nullptr || reply->type == REDIS_REPLY_ERROR) {
		std::cout << "认证失败" << std::endl;
		//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}
	else {
		//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
		freeReplyObject(reply);
		std::cout << "认证成功" << std::endl;
		con_pool_->returnConnection(connect);
		return true;
	}
}

// 从列表头部插入一个值
bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}

	auto reply = (redisReply*)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
	if (nullptr == reply)
	{
		std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}

	std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] success ! " << std::endl;
	freeReplyObject(reply);
	con_pool_->returnConnection(connect);
	return true;
}

// 从列表头部弹出一个值
bool RedisMgr::LPop(const std::string& key, std::string& value) {
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "LPOP %s ", key.c_str());
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
		std::cout << "Execut command [ LPOP " << key << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}
	value = reply->str;
	std::cout << "Execut command [ LPOP " << key << " ] success ! " << std::endl;
	freeReplyObject(reply);
	con_pool_->returnConnection(connect);
	return true;
}

// 从列表尾部插入一个值
bool RedisMgr::RPush(const std::string& key, const std::string& value) {
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
	if (nullptr == reply)
	{
		std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}

	std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] success ! " << std::endl;
	freeReplyObject(reply);
	con_pool_->returnConnection(connect);
	return true;
}

// 从列表尾部弹出一个值
bool RedisMgr::RPop(const std::string& key, std::string& value) {
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "RPOP %s ", key.c_str());
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
		std::cout << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}
	value = reply->str;
	std::cout << "Execut command [ RPOP " << key << " ] success ! " << std::endl;
	freeReplyObject(reply);
	return true;
}

// 一般存储字符串
bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value) {
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
	if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}
	std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] success ! " << std::endl;
	freeReplyObject(reply);
	con_pool_->returnConnection(connect);
	return true;
}

// 一般用于存储二进制数据
bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	const char* argv[4];
	size_t argvlen[4];
	argv[0] = "HSET";
	argvlen[0] = 4;
	argv[1] = key;
	argvlen[1] = strlen(key);
	argv[2] = hkey;
	argvlen[2] = strlen(hkey);
	argv[3] = hvalue;
	argvlen[3] = hvaluelen;
	auto reply = (redisReply*)redisCommandArgv(connect, 4, argv, argvlen);
	if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}
	std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] success ! " << std::endl;
	freeReplyObject(reply);
	return true;
}

// 根据一二级key获取值
std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return "";
	}
	const char* argv[3];
	size_t argvlen[3];
	argv[0] = "HGET";
	argvlen[0] = 4;
	argv[1] = key.c_str();
	argvlen[1] = key.length();
	argv[2] = hkey.c_str();
	argvlen[2] = hkey.length();
	auto reply = (redisReply*)redisCommandArgv(connect, 3, argv, argvlen);
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		std::cout << "Execut command [ HGet " << key << " " << hkey << "  ] failure ! " << std::endl;
		return "";
	}

	std::string value = reply->str;
	freeReplyObject(reply);
	con_pool_->returnConnection(connect);
	std::cout << "Execut command [ HGet " << key << " " << hkey << " ] success ! " << std::endl;
	return value;
}

// 删除某个key
bool RedisMgr::Del(const std::string& key)
{
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "DEL %s", key.c_str());
	if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ Del " << key << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}
	std::cout << "Execut command [ Del " << key << " ] success ! " << std::endl;
	freeReplyObject(reply);
	con_pool_->returnConnection(connect);
	return true;
}

// 判断key是否存在
bool RedisMgr::ExistsKey(const std::string& key)
{
	auto connect = con_pool_->getConnection();
	if (connect == nullptr)
	{
		std::cout << "connect redis failed" << std::endl;
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "exists %s", key.c_str());
	if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
		std::cout << "Not Found [ Key " << key << " ]  ! " << std::endl;
		freeReplyObject(reply);
		con_pool_->returnConnection(connect);
		return false;
	}
	std::cout << " Found [ Key " << key << " ] exists ! " << std::endl;
	freeReplyObject(reply);
	con_pool_->returnConnection(connect);
	return true;
}

void RedisMgr::Close()
{
	con_pool_->Close();
}

