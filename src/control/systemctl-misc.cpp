#include <cstring>
#include <filesystem>
#include "events.h"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <rang.hpp>
#include <format>
#include <sys/un.h>
#include "rang.hpp"
using namespace rang;

void l_initpoweroff(const std::string& operation)
{
	if (getuid() != 0)
	{
		std::cout << fg::red << "Error: " << fg::blue << "You must run this program as root." << fg::reset << std::endl;
		exit(1);
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
	{
		error(std::format("Could not create socket: {}", strerror(errno)));
		exit(1);
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/run/zinit/init.sock");
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		error(std::format("Could not connect to socket: {}", strerror(errno)));
		close(fd);
		exit(1);
	}
	write(fd, operation.c_str(), operation.length());
	std::string response;
	int n = recv(fd, &response, response.size(), 0);
	if (n > 0) std::cout << response;
	close(fd);
	exit(1);
}