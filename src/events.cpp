#include <iostream>
#include <unistd.h>
#include <csignal>
#include <sys/reboot.h>
#include "rang.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
using namespace rang;

void info(const std::string& msg)
{
	setControlMode(control::Force); // from namespace rang
	std::cout << "[  " << fg::green << "INFO" << fg::reset << "  ] " << msg << std::endl;
}

void error(const std::string& msg)
{
	setControlMode(control::Force); // from namespace rang
	std::cerr << "[  " << fg::red << "ERROR" << fg::reset << "  ] " << msg << std::endl;
}

void warn(const std::string& msg)
{
	setControlMode(control::Force); // also from namespace rang
	std::cerr << "[  " << fg::yellow << "WARN" << fg::reset << "  ] " << msg << std::endl;
}

void servicedinfo(const std::string& msg)
{
	setControlMode(control::Force); // :)
	std::cout << "[  " << fg::green << "SERVICED" << fg::reset << "  ] " << msg << std::endl;
}
void debuginfo(const std::string& msg)
{
	setControlMode(control::Force);
	std::cout << "[  " << fg::blue << "DEBUG" << fg::reset << "  ] " << msg << std::endl;
}

void initreboot()
{
	info("Rebooting system");
	std::cout << "Sending SIGTERM to all processes..." << std::endl;
	kill(-1, SIGTERM);
	sleep(5);

	std::cout << "Sending SIGKILL to all processes..." << std::endl;
	kill(-1, SIGKILL);

	std::cout << "Remounting root to read-only..." << std::endl;
	if (mount(nullptr, "/", nullptr, MS_REMOUNT | MS_RDONLY, nullptr) == -1)
	{
		warn(std::format("Can't remount root: {}", strerror(errno)));
	}

	std::cout << "Syncing..." << std::endl;
	sync();
	reboot(RB_AUTOBOOT);

	exit(0);
}

void inithalt()
{
	info("Shuting down system");
	std::cout << "Sending SIGTERM to all processes..." << std::endl;
	kill(-1, SIGTERM);
	sleep(5);

	std::cout << "Sending SIGKILL to all processes..." << std::endl;
	kill(-1, SIGKILL);

	std::cout << "Remounting root to read-only..." << std::endl;
	if (mount(nullptr, "/", nullptr, MS_REMOUNT | MS_RDONLY, nullptr) == -1)
	{
		warn(std::format("Can't remount root: {}", strerror(errno)));
	}

	std::cout << "Syncing..." << std::endl;
	sync();
	reboot(RB_POWER_OFF);

	exit(0);
}

void spawn_emergencyshell()
{
	pid_t pid = fork();
	if (pid == 0)
	{
		char *argv[] = {"sh", nullptr};
		execve("/bin/sh", argv, nullptr);

		error(std::format("execve() failed: {}", strerror(errno)));

		std::cout << "Rebooting in 3 seconds." << std::endl;
		sleep(3);

		initreboot();
	}
	else if (pid > 0)
	{
		int status;
		pid_t w = waitpid(pid, &status, 0);

		if (w == -1)
		{
			error(std::format("waitpid() failed: {}", strerror(errno)));
			std::cout << "Rebooting in 3 seconds." << std::endl;
			sleep(3);
			initreboot();
		}

		if (WIFEXITED(status))
		{
			std::cout << "Shell exited with code " << WEXITSTATUS(status) << std::endl;
			std::cout << "Rebooting in 3 seconds." << std::endl;
			sleep(3);
			initreboot();
		}
	}
	else
	{
		error(std::format("fork() failed: {}", strerror(errno)));
		std::cout << "Rebooting in 3 seconds." << std::endl;
		sleep(3);
		initreboot();
	}


	std::cout << "Rebooting in 3 seconds." << std::endl;
	sleep(3);
	initreboot();
}

void panic(const std::string& msg)
{
	error(msg);

	while (true)
	{
		std::cout << "1 - Reboot" << std::endl;
		std::cout << "2 - Enter /bin/sh (emergency shell)" << std::endl;
		std::cout << "Select an option: ";
		std::string option;
		std::cin >> option;

		if (option == "1")
		{
			initreboot();
			break;
		}

		if (option == "2")
		{
			spawn_emergencyshell();
			break;
		}
		else
		{
			std::cout << std::endl << "Invalid option" << std::endl;
			sleep(1);
		}
	}
}