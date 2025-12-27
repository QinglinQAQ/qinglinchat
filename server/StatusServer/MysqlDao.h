#pragma once
#include "const.h"
class MysqlPool {
public:
	MysqlPool(const std::string&, const std::string&, const std::string&, const std::string&, int);
	std::unique_ptr<sql::Connection> getConnection();
	void returnConnection(std::unique_ptr<sql::Connection>);
	void Close();
	~MysqlPool();
private:
	std::string url_; // 服务器地址
	std::string user_; // 用户名
	std::string password_; // 密码
	std::string schema_; // 数据库名
	int poolSize_; // 连接池大小
	std::atomic<bool> b_stop_; // 连接池是否停止工作
	std::queue<std::unique_ptr<sql::Connection>> pool_;
	std::mutex mutex_; // 互斥锁
	std::condition_variable cond_; // 条件变量
};

struct UserInfo {
	std::string name;
	std::string pwd;
	int uid;
	std::string email;
};

class MysqlDao
{
public:
	MysqlDao();
	~MysqlDao();
	int RegisterUser(const std::string&, const std::string&, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePwd(const std::string& name, const std::string& newpwd);
	bool CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo);
private:
	std::unique_ptr<MysqlPool> con_pool_; // 连接池
};

