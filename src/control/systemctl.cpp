#include <cstring>
#include <filesystem>
#include "events.h"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <rang.hpp>
#include <unordered_map>
#include <pwd.h>
#include <format>
#include <sys/un.h>
#include <vector>
#include "systemctl/systemctl-misc.h"
#include "sserver/init-socket.h"
#include "serviced.h"
#include "info.h"
#include <iterator>
using namespace rang;

std::unordered_map<std::string, std::string> commands;

void printHelp()
{
	printVersion();
	std::cout << std::endl << fg::blue << "Usage: systemctl [options] [command]" << std::endl << std::endl;
	std::cout << fg::blue << "Options:" << fg::reset << std::endl;
	//std::cout << "--user: runs as normal user." << std::endl; // I'll implement that later
	std::cout << "--version: shows version." << std::endl;
	std::cout << "--help: shows help." << std::endl << std::endl;
	std::cout << fg::blue << "Commands:" << fg::reset << std::endl;
	std::cout << "reboot: reboots the system" << std::endl;
	std::cout << "poweroff: shut downs the system" << std::endl;
	std::cout << "halt: alias to poweroff" << std::endl;
	std::cout << "start [SERVICE]: starts SERVICE" << std::endl;
	std::cout << "stop [SERVICE]: stops SERVICE" << std::endl;
	std::cout << "restart [SERVICE]: restarts SERVICE" << std::endl;
	std::cout << "status [SERVICE (optional)]: shows status of SERVICE" << std::endl;
	std::cout << "refresh: refreshes services" << std::endl << std::endl;
	std::cout << fg::blue << "Homepage: https://github.com/zos-linux/zinit" << fg::reset << std::endl;
	exit(0);
}

bool is_valid(const std::string& cmd)
{
	std::vector<std::string> valid_commands = {"poweroff", "halt", "reboot", "start", "stop", "restart", "status", "refresh"};
	for (const std::string &command : valid_commands)
	{
		if (command == cmd)
			return true;
	}
	return false;
}

int main(int argc, char* argv[])
{
	commands["poweroff"] = "INIT_POWEROFF";
	commands["halt"] = commands.at("poweroff");
	commands["reboot"] = "INIT_REBOOT";
	commands["start"] = "SD_SVSTART";
	commands["stop"] = "SD_SVSTOP";
	commands["restart"] = "SD_SVRESTART";
	commands["status"] = "SD_SVSTATUS";
	commands["refresh"] = "SD_SVREFRESH";

	std::unordered_map<std::string, std::string> argMap;
	if (strcmp(argv[0], "halt") == 0 || strcmp(argv[0], "poweroff") == 0)
	{
		l_initpoweroff(commands.at(argv[0]));
	}
	else if (strcmp(argv[0], "reboot") == 0)
	{
		l_initpoweroff(commands.at(argv[0]));
	}

	if (argc < 2) { printHelp(); return 0; }

	Command command;
	command.cmd.clear();
	for (int i=1; i < argc; i++)
	{
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
			printHelp();
			return 0;
		}
		else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0)
		{
			printVersion();
			std::cout << "Licensed under GNU General Public License v3." << std::endl;
			std::cout << "zinit comes with ABSOLUTELY NO WARRANTY to the extent permitted by applicable law." << std::endl;
			std::cout << "Report bugs on issues page on https://github.com/zos-linux/zinit" << std::endl;
			exit(0);
		}
		else if (strcmp(argv[i], "--user") == 0 || strcmp(argv[i], "-u") == 0)
		{
			uid_t uid = getuid();
			if (uid == 0)
			{
				error("systemctl cannot work in user mode while running as root.");
				return 1;
			}

			passwd *pw = getpwuid(uid);
			if (pw == 0)
				argMap["USER"];
			else
			{
				std::cout << fg::red << "Error: " << fg::blue << "Unknown uid: " << fg::reset << uid << std::endl;
				return 1;
			}
		}
		else if (is_valid(argv[i]))
		{
			if (command.cmd.empty())
			{
				command.cmd = commands.at(argv[i]);
				if (strcmp(argv[i], "poweroff") != 0 && strcmp(argv[i], "halt") != 0 && strcmp(argv[i],  "reboot") != 0 && strcmp(argv[i], "refresh") != 0)
				{
					if (argc == i + 1)
					{
						error("Second argument needed.");
						exit(1);
					}
					command.arg = argv[i + 1];
				}
				else {command.arg = "justforcase";}
			}
		}
		else
		{
			if (strcmp(argv[i], command.arg.c_str()) != 0)
			{
				error(std::format("Invalid command or option: {}", argv[i]));
				exit(1);
			}
		}
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
	{
		error(std::format("Could not create socket: {}", strerror(errno)));
		return 1;
	}
	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/run/zinit/init.sock");
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		error(std::format("Could not connect to socket: {}", strerror(errno)));
		close(fd);
		return 1;
	}

	std::string json_command = command_to_json(command);
	send(fd, json_command.c_str(), json_command.size(), 0);

	ssize_t n;
	Service service;
	std::string json_service;
	std::string response;

	response.resize(4096);
	json_service.resize(4096);
	if (command.cmd == "SD_SVSTATUS")
	{
		n = recv(fd, json_service.data(), json_service.size(), 0);
	}
	else
	{
		n = recv(fd, response.data(), response.size(), 0);
	}

	if (n > 0)
	{
		if (command.cmd == "SD_SVSTATUS")
		{
			service = json_to_service(json_service);
			if (service.name == "UNKNOWN") { std::cout << fg::red << "Error: " << fg::blue << "Specified service doesn't exist" << fg::reset << std::endl; }
			else
			{
				std::cout << fg::blue << service.name << fg::reset << std::endl;
				std::cout << "  ⸠ " << fg::blue << "Service type: " << fg::reset << service.type << std::endl;
				std::cout << "  ⸠ " << fg::blue << "Dependencies: " << fg::reset;
				for (const auto& dep : service.deps ) { std::cout << dep; std::cout << " "; } std::cout << std::endl;
				std::cout << "  ⸠ " << fg::blue << "Running: ";
				if (service.running) { std::cout << fg::green << "Yes" << fg::reset << std::endl; } else { std::cout << fg::red << "No" << fg::reset << std::endl; }
				std::cout << "  ⸠ " << fg::blue << "Pid: " << fg::reset;
				if (service.running) { std::cout << service.pid << std::endl; } else { std::cout << "N/A" << std::endl; }
				std::cout << "  ⎣ " << fg::blue << "Note: " << fg::reset << service.note << std::endl;
			}
		}
		else
		{
			std::cout << fg::blue << "INFO: " << fg::reset << response;
		}
	}

	close(fd);
	return 0;
}