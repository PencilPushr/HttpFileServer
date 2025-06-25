#include "Config.h"

Config::Config()
{

}

int Config::GetPort()
{
	return 0;
}

const std::string* Config::GetRootDir()
{
	return nullptr;
}

const std::string* Config::GetWebDir()
{
	return nullptr;
}

bool Config::ParseEnv()
{
	return false;
}
