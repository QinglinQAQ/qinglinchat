#pragma once
#include "const.h"
#include "hiredis.h"

class RedisConPool {
public:
	RedisConPool(size_t poolSize, const char* host, int port, const char* password); // 构造函数
	~RedisConPool();
	redisContext* getConnection(); // 获取连接
	void returnConnection(redisContext* conn); // 归还连接
	void Close(); // 关闭连接池
private:
	std::atomic<bool> b_stop_; // 连接池是否停止工作
	size_t poolSize_;// 连接池大小
	std::string host_; // 服务器地址
	int port_; // 服务器端口
	std::string password_; // 认证密码
	std::queue<redisContext*> connections_; // 连接队列
	std::mutex mutex_; // 互斥锁
	std::condition_variable cond_; // 条件变量
};

class RedisMgr : public Singleton<RedisMgr>,
	public std::enable_shared_from_this<RedisMgr>
{
	friend class Singleton<RedisMgr>;
public:
	~RedisMgr();
	bool Connect(const std::string& host, int port);
	bool Get(const std::string& key, std::string& value);
	bool Set(const std::string& key, const std::string& value);
	bool Auth(const std::string& password);
	bool LPush(const std::string& key, const std::string& value);
	bool LPop(const std::string& key, std::string& value);
	bool RPush(const std::string& key, const std::string& value);
	bool RPop(const std::string& key, std::string& value);
	bool HSet(const std::string& key, const std::string& hkey, const std::string& value);
	bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
	std::string HGet(const std::string& key, const std::string& hkey);
	bool HDel(const std::string& key, const std::string& field);
	bool Del(const std::string& key);
	bool ExistsKey(const std::string& key);
	void Close();
private:
	RedisMgr();
	std::unique_ptr< RedisConPool>  con_pool_; // 连接池
};