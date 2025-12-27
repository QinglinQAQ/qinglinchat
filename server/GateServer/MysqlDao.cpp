#include "MysqlDao.h"
#include "ConfigMgr.h"

MysqlPool::MysqlPool(const std::string& url,
	const std::string& user,
	const std::string& password,
	const std::string& schema,
	int poolSize)
	: url_(url), user_(user), password_(password), poolSize_(poolSize), schema_(schema), b_stop_(false) {
	try {
		std::cout << url_ << std::endl;
		std::string tcp_url_ = "tcp://" + url_;
		std::cout << schema_ << std::endl;
		for (int i = 0;i < poolSize_; ++i) {
			sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
			std::unique_ptr<sql::Connection> conn(
				driver->connect(url_, user_, password_)); // 创建连接
			conn->setSchema(schema_); // 选择数据库
			pool_.push(std::move(conn)); // 放入连接池
		}
	}
	catch (sql::SQLException& e) {
		std::cout << user_ << ' ' << ' ' << password_ << std::endl;
		std::cerr << "MysqlPool init error: " << e.what() << std::endl;
	}
}

std::unique_ptr<sql::Connection> MysqlPool::getConnection() {
	std::unique_lock<std::mutex> lock(mutex_); // 加锁
	cond_.wait(lock, [this] { // 等待条件变量
		if (b_stop_) {
			return true;
		}
		return !pool_.empty();
		});
	//如果停止则直接返回空指针
	if (b_stop_) {
		return nullptr;
	}
	std::unique_ptr<sql::Connection> conn = std::move(pool_.front());
	pool_.pop();
	return conn;
}

void MysqlPool::returnConnection(std::unique_ptr<sql::Connection> conn) {
	std::lock_guard<std::mutex> lock(mutex_); // 加锁
	pool_.push(std::move(conn)); // 放回连接池
	cond_.notify_one(); // 通知等待线程
}

void MysqlPool::Close() {
	b_stop_ = true;
	cond_.notify_all(); // 通知所有等待线程
}
MysqlPool::~MysqlPool() {
	std::unique_lock<std::mutex> lock(mutex_); // 加锁
	while (!pool_.empty()) {
		pool_.pop();
	}
}

MysqlDao::MysqlDao()
{
	auto& cfg = ConfigMgr::Inst();
	const auto& host = cfg["Mysql"]["Host"];
	const auto& port = cfg["Mysql"]["Port"];
	const auto& pwd = cfg["Mysql"]["Passwd"];
	const auto& schema = cfg["Mysql"]["Schema"];
	const auto& user = cfg["Mysql"]["User"];
	con_pool_.reset(new MysqlPool(host + ":" + port, user, pwd, schema, 5));
}
MysqlDao::~MysqlDao() {
	con_pool_->Close();
}

int MysqlDao::RegisterUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	auto conn = con_pool_->getConnection();// 获取连接
	try {
		if(conn==nullptr){
			//con_pool_->returnConnection(std::move(conn)); 
			return false;
		}
		// 预处理存储过程调用
		std::unique_ptr < sql::PreparedStatement > stmt(conn->prepareStatement("CALL reg_user(?,?,?,@result)"));
		// 设置输入参数
		stmt->setString(1, name);
		stmt->setString(2, email);
		stmt->setString(3, pwd);
		stmt->execute(); // 执行存储过程
		std::unique_ptr<sql::Statement> stmtResult(conn->createStatement()); // 创建语句对象
		std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result")); // 获取输出参数
		if (res->next()) {
			int result = res->getInt("result");
			std::cout << "GET result=" << result << std::endl;
			con_pool_->returnConnection(std::move(conn));
			return result; // 返回结果
		}
		else {
			con_pool_->returnConnection(std::move(conn));
			std::cout << "未获取到结果" << std::endl;
			return -1; // 未获取到结果
		}
	}
	catch (sql::SQLException& e) {
		con_pool_->returnConnection(std::move(conn));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return -1;
	}
	return 0;
}

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email) {
	auto con = con_pool_->getConnection();
	try {
		if (con == nullptr) {
			con_pool_->returnConnection(std::move(con));
			return false;
		}
		// 准备查询语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT email FROM user WHERE name = ?"));
		// 绑定参数
		pstmt->setString(1, name);
		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// 遍历结果集
		while (res->next()) {
			std::cout << "Check Email: " << res->getString("email") << std::endl;
			if (email != res->getString("email")) {
				con_pool_->returnConnection(std::move(con));
				return false;
			}
			con_pool_->returnConnection(std::move(con));
			return true;
		}
	}
	catch (sql::SQLException& e) {
		con_pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo) {
	auto con = con_pool_->getConnection();
	//std::cout << "email:" << email << std::endl;
	if (con == nullptr) {
		//std::cout << " MysqlDao::CheckPwd con falied" << std::endl;
		return false;
	}

	Defer defer([this, &con]() { // 类似Go语言的defer机制，确保函数结束时归还连接
		con_pool_->returnConnection(std::move(con));
		});

	try {


		// 准备SQL语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT * FROM user WHERE email = ?"));
		pstmt->setString(1, email); // 将username替换为你要查询的用户名

		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::string origin_pwd = "";
		// 遍历结果集
		while (res->next()) {
			origin_pwd = res->getString("pwd");
			// 输出查询到的密码
			//std::cout << "Password: " << origin_pwd << std::endl;
			break;
		}
		//std::cout << "origin_pwd:" << origin_pwd << std::endl;
		if (pwd != origin_pwd) {
			return false;
		}
		userInfo.name = res->getString("name");
		userInfo.email = res->getString("email");
		userInfo.uid = res->getInt("uid");
		userInfo.pwd = origin_pwd;
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd) {
	auto con = con_pool_->getConnection();
	try {
		if (con == nullptr) {
			con_pool_->returnConnection(std::move(con));
			return false;
		}
		// 准备查询语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));
		// 绑定参数
		pstmt->setString(2, name);
		pstmt->setString(1, newpwd);
		// 执行更新
		int updateCount = pstmt->executeUpdate();
		std::cout << "Updated rows: " << updateCount << std::endl;
		con_pool_->returnConnection(std::move(con));
		return true;
	}
	catch (sql::SQLException& e) {
		con_pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}