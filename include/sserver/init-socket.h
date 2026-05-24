#ifndef INIT_SOCKET_H
#define INIT_SOCKET_H

#define INIT_HALT "INIT_POWEROFF"

#include <string>
#include "serviced.h"

struct Command
{
	std::string cmd; // INIT_POWEROFF, SD_SVSTART etc.
	std::string arg;
};

int init_main_socket(int& server_fd);
int init_serviced_socket(int& server_fd);

std::string service_to_json(const Service& sv);
Service json_to_service(const std::string& json);
std::string command_to_json(const Command& command);
Command json_to_command(const std::string& json);

#endif