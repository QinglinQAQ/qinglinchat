#include "MysqlMgr.h"

MysqlMgr::MysqlMgr()
{
}

MysqlMgr::~MysqlMgr()
{
}

int MysqlMgr::RegisterUser(const std::string& name, const std::string& email, const std::string& pwd) {
	return dao_.RegisterUser(name, email, pwd);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email)
{
	return dao_.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string& name, const std::string& pwd)
{
	return dao_.UpdatePwd(name, pwd);
}

bool MysqlMgr::CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo)
{
	//std::cout << "MysqlMgr::CheckPwd--->: " << pwd << std::endl;
	return dao_.CheckPwd(email, pwd, userInfo);
	return false;
}
