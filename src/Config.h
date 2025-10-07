#pragma once

#include <string>
#include <fstream>
#include <unordered_set>

class Config
{
public:
	int port = 8080;
	std::string rootDir = "./files";
	std::string webDir = "./web";
	std::string logFile = "server.log";
	bool enable_cors = true;
	bool enable_logging = true;
	size_t max_upload_size = 0xFFFFFFFFFFFFFFFF;  // 10MB default
	size_t max_uploads_per_request = 10;
	std::unordered_set<std::string> blacklisted_extensions = {
		".malware"
	};
	bool allow_overwrite = false;
	std::string upload_temp_dir = "./temp";  // Temporary directory for uploads
public:
	Config();

	int GetPort();
	const std::string* GetRootDir();
	const std::string* GetWebDir();
private:
	bool ParseEnv();
};
