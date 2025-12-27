#pragma once
#include "const.h"

class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> httpHandler;
class LogicSystem : public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem() = default;
	bool HandleGet(std::string, std::shared_ptr<HttpConnection> );
	bool HandlePost(std::string, std::shared_ptr<HttpConnection>);
	void RegisterGet(std::string, httpHandler);
	void RegisterPost(std::string, httpHandler);
private:
	LogicSystem();
	std::map<std::string, httpHandler> _post_handlers;
	std::map<std::string, httpHandler> _get_handlers;
};

