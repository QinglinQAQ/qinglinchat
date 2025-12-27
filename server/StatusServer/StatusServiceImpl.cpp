#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include <boost/uuid/uuid.hpp>            // uuid 类型定义
#include <boost/uuid/uuid_generators.hpp> // UUID 生成器（随机、名称等）
#include <boost/uuid/uuid_io.hpp>         // 流输出支持（operator<<）

std::string generate_unique_string() {
    // 创建UUID对象
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    // 将UUID转换为字符串
    std::string unique_string = to_string(uuid);
    return unique_string; //
}

StatusServiceImpl::StatusServiceImpl()
{
    auto& cfg = ConfigMgr::Inst();
    ChatServer server;

    std::vector<std::string> words;
    std::stringstream ss(cfg["Chatservers"]["Name"]);
    std::string word;
    while (std::getline(ss, word, ',')) {
        //std::cout << word << std::endl;
        words.push_back(word);
    }
    for (auto& word : words) {
        //std::cout << cfg[word]["Name"] << std::endl;
        if (cfg[word]["Name"].empty())
            continue;
        _servers[word] = ChatServer(cfg[word]["Host"], cfg[word]["Port"], cfg[word]["Name"]);
        //std::cout << _servers[word].name << ' ' << _servers[word].host << ' ' << _servers[word].port << std::endl;
    }
}


Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* reply)
{
    std::string prefix("qinglin status server has received :  ");
    std::cout << prefix << ' ' << request->uid() << std::endl;
    const auto& server = getChatServer();
	std::cout << server.host << ":" << server.port << std::endl;
    reply->set_host(server.host);
    reply->set_port(server.port);
    reply->set_error(ErrorCodes::Success);
    reply->set_token(generate_unique_string());
    std::cout << "insterTorken("<<request->uid()<<","<<reply->token()<<")" << std::endl;
    insertToken(request->uid(), reply->token());
    std::cout << "OK" << std::endl;
    return Status::OK;
}

Status StatusServiceImpl::Login(ServerContext* context, const LoginReq* request, LoginRsp* reply)
{
    auto uid = request->uid();
    auto token = request->token(); 

    std::string uid_str = std::to_string(uid);
    std::string token_key = USERTOKENPREFIX + uid_str;
    std::string token_value = "";

    bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
    if (!success) {
        reply->set_error(ErrorCodes::UidInvalid);
        return Status::OK;
    }

    if (token_value != token) {
        reply->set_error(ErrorCodes::TokenInvalid);
        return Status::OK;
    }
    reply->set_error(ErrorCodes::Success);
    reply->set_uid(uid);
    reply->set_token(token);
    return Status::OK;

}

ChatServer StatusServiceImpl::getChatServer()
{
    std::lock_guard<std::mutex>  guard(_server_mtx); // 加锁

    auto minServer = _servers.begin()->second;
    auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, minServer.name); // 获取连接数量, 没有则为空
    if (count_str.empty()) {
        minServer.con_count = INT_MAX;
    }
    else {
        minServer.con_count = std::stoi(count_str);
    }


    // 使用范围基于for循环 目前两个聊天服务器
    for (auto& server : _servers) {

        if (server.second.name == minServer.name) {
            continue;
        }

        auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server.second.name);
        if (count_str.empty()) {
            server.second.con_count = INT_MAX;
        }
        else {
            server.second.con_count = std::stoi(count_str);
        }

        if (server.second.con_count < minServer.con_count) {
            minServer = server.second;
        }
    }
    std::cout << "getChatServer--->" << minServer.name << std::endl;
    return minServer;
}
void StatusServiceImpl::insertToken(int uid, std::string token)
{
    std::string uid_str = std::to_string(uid);
    std::string token_key = USERTOKENPREFIX + uid_str; // utoken_
    RedisMgr::GetInstance()->Set(token_key, token);
}