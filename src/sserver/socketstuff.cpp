#include "events.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <format>
#include <string>
#include <iostream>
#include "sserver/init-socket.h"
#include <nlohmann/json.hpp>
#include "serviced.h"

int init_main_socket(int& server_fd)
{
	info("Opening an UNIX socket in the /run/zinit/init.sock file");
	server_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	if (server_fd < 0)
	{
		error(std::format("Can't open UNIX socket: {}", strerror(errno)));
		return 1;
	}

	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/run/zinit/init.sock");
	unlink("/run/zinit/init.sock");

	if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
		error(std::format("Can't bind UNIX socket: {}", strerror(errno)));
		return 1;
	}

	if (listen(server_fd, 5) < 0) {
		error(std::format("Can't open UNIX socket: {}", strerror(errno)));
		return 1;
	}

	return 0;
}

int init_serviced_socket(int& server_fd)
{
	info("Opening an UNIX socket in the /run/zinit/serviced.sock file");
	server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (server_fd < 0)
	{
		error(std::format("Can't open UNIX socket: {}", strerror(errno)));
		return 1;
	}

	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/run/zinit/serviced.sock");
	unlink("/run/zinit/serviced.sock");

	if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
		error(std::format("Can't bind UNIX socket: {}", strerror(errno)));
		return 1;
	}

	if (listen(server_fd, 5) < 0)
	{
		error(std::format("Can't listen on socket: {}", strerror(errno)));
		return 1;
	}

	return 0;
}

std::string service_to_json(const Service& sv)
{
	nlohmann::json j;
	j["name"] = sv.name;
	j["type"] = sv.type;
	j["command"] = sv.command;
	j["deps"] = sv.deps;
	j["autorestart"] = sv.autorestart;
	j["running"] = sv.running;
	j["pid"] = sv.pid;
	j["restartcount"] = sv.restartcount;
	j["note"] = sv.note;
	j["setsid"] = sv.setsid;
	return j.dump();
}

Service json_to_service(const std::string& json)
{
	const nlohmann::json j = nlohmann::json::parse(json);
	Service sv;
	sv.name = j["name"];
	sv.type = j["type"];
	sv.command = j["command"];
	sv.deps = j["deps"];
	sv.autorestart = j["autorestart"];
	sv.running = j["running"];
	sv.pid = j["pid"];
	sv.restartcount = j["restartcount"];
	sv.note = j["note"];
	sv.setsid = j["setsid"];
	return sv;
}

std::string command_to_json(const Command& command)
{
	nlohmann::json j;
	j["cmd"] = command.cmd;
	j["arg"] = command.arg;
	return j.dump();
}

Command json_to_command(const std::string& json)
{
	const nlohmann::json j = nlohmann::json::parse(json);
	Command command;
	command.cmd = j.at("cmd");
	command.arg = j.at("arg");
	return command;
}