#ifndef SERVICED_H
#define SERVICED_H
#include <vector>
#include <string>


struct Service
{
	std::string name;
	std::string command;
	std::string type;
	std::vector<std::string> deps;

	bool autorestart = true;
	bool running = false;
	int pid = -1;

	int restartcount = 1;
	std::string note;
	bool setsid = false;
};

struct Target
{
	std::string name;
	std::vector<Service> services;
};

struct GraphResult {
	std::vector<std::string> order;
	std::vector<std::string> failed;
};

#endif
