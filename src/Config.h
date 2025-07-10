#pragma once

#include <string>
#include <fstream>
#include <vector>

class Config
{
	// Public for now, but these will go back to private in V1.1
public:
	int port = 8080;
	std::string rootDir = "./files";
	std::string webDir = "./web";
	std::string logFile = "server.log";
	bool enable_cors = true;
	bool enable_logging = true;
	size_t max_upload_size = 10 * 1024 * 1024;  // 10MB default
	size_t max_uploads_per_request = 10;
	std::vector<std::string> allowed_extensions = {
		".txt", ".pdf", ".jpg", ".jpeg", ".png", ".gif",
		".doc", ".docx", ".xls", ".xlsx", ".zip", ".csv"
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
