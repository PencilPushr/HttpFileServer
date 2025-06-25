#pragma once

#include <string>
#include <fstream>

class Config
{
private:
	int port = 8080;
	std::string rootDir = "./files";
	std::string webDir = "./web";
	std::string logFile = "server.log";
	bool enable_cors = true;
	bool enable_logging = true;
public:
	Config();

	int GetPort();
	const std::string* GetRootDir();
	const std::string* GetWebDir();
private:
	bool ParseEnv();
};
