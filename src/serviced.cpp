#include <filesystem>
#include <unordered_map>
#include "events.h"
#include "parse-file.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <format>
#include "serviced-file-management.h"
#include "serviced.h"
#include <iostream>
#include <queue>
#include <unordered_set>
#include <functional>
#include <thread>
#include "sserver/init-socket.h"
#include "serviced-svu.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "sserver/init-socket.h"

void sigchldhandler([[maybe_unused]] int sig)
{
	int status;
	[[maybe_unused]] pid_t process_pid;
	while ((process_pid = waitpid(-1, &status, WNOHANG)) > 0);
}

// it should look more polished over time
// now just sets default hardcoded settings
void checkconfigmap(std::unordered_map<std::string, std::string>& configmap)
{
	if (!configmap.contains("SV_START_DELAY"))
	{
		configmap["SV_START_DELAY"] = std::to_string(50);
	}
}

int start_service(Service& s)
{
	if (s.restartcount == 1)
		info(std::format("Starting {}...", s.name));

	pid_t pid = fork();
	if (pid < 0)
	{
		s.running = false;
		s.note = "Not running (fork() failed)";
		return 1;
	}
	if (pid == 0)
	{
		execl("/bin/sh", "sh", "-c", s.command.c_str(), nullptr);
		_exit(1);
	}

	s.pid = pid;
	s.running = true;
	s.note = "Running.";
	return 0;
}

std::vector<Service> load_services()
{
	std::vector<std::filesystem::path> fileList;
	if (!std::filesystem::exists("/etc/zinit.d")) { error("Can't find /etc/zinit.d"); exit(1); }

	{
		bool notempty = false;
		for (const auto& entry : std::filesystem::directory_iterator("/etc/zinit.d"))
		{
			if (entry.is_regular_file())
				notempty = true;
		}
		if (!notempty) { error("/etc/zinit.d is empty"); exit(1); }
	}

	for (const auto& file : std::filesystem::directory_iterator("/etc/zinit.d"))
	{
		if (file.is_regular_file())
		{
			fileList.push_back(file.path());
		}
	}

	std::vector<Service> services;
	for (const auto& file : fileList)
	{
		Service s;
		if (checkservicefile(parseFile(file), &s) != 0)
		{
			error(std::format("Could not load service file: {}", file.string()));
			continue;
		}

		services.push_back(s);
	}
	return services;
}

void refresh_services(std::vector<Service>& services)
{
	std::vector<Service> newservices = load_services();
	for (Service& s : newservices)
	{
		if (!service_exists(s.name, services))
			services.push_back(s);
		else
		{
			Service* sv = &find_sv_by_name(s.name, services);
			if (sv->type != s.type)
				sv->type = s.type;
			if (sv->command != s.command)
				sv->command = s.command;
			if (sv->deps != s.deps)
				sv->deps = s.deps;
		}
	}
}

GraphResult build_and_sort_services(std::vector<Service>& services)
{
	std::unordered_map<std::string, Service*> map;
	std::unordered_map<std::string, int> indegree;
	std::unordered_map<std::string, std::vector<std::string>> graph;

	std::unordered_set<std::string> failed;

	// init
	for (auto &s : services)
	{
		map[s.name] = &s;
		indegree[s.name] = 0;
	}

	// build graph
	for (auto &s : services)
	{
		for (const auto &dep : s.deps)
		{
			if (dep == "none")
				continue;

			if (!map.contains(dep))
			{
				failed.insert(dep);
				failed.insert(s.name);
				continue;
			}

			graph[dep].push_back(s.name);
			indegree[s.name]++;
		}
	}

	// queue
	std::queue<std::string> q;

	for (auto &[name, deg] : indegree)
	{
		if (deg == 0)
			q.push(name);
	}

	std::vector<std::string> order;

	while (!q.empty())
	{
		auto cur = q.front();
		q.pop();

		order.push_back(cur);

		for (auto &next : graph[cur])
		{
			if (--indegree[next] == 0)
				q.push(next);
		}
	}

	// cycle detection
	for (auto &[name, deg] : indegree)
	{
		if (deg > 0)
			failed.insert(name);
	}

	return {
		order,
		std::vector<std::string>(failed.begin(), failed.end())
	};
}

int make_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void stop_service(Service& s)
{
	if (s.pid <= 0)
		return;

	s.autorestart = false;
	kill(s.pid, SIGTERM);

	int status = 0;
	pid_t r = waitpid(s.pid, &status, WNOHANG);

	if (r == 0)
	{
		// still alive
		kill(s.pid, SIGKILL);
		waitpid(s.pid, &status, 0);
	}

	s.running = false;
	s.pid = -1;
	// this was a triumph
	// I'm making a note here:
	// s.note = "HUGE SUCCESS";
}

int main()
{
	signal(SIGTERM, SIG_IGN);

	if (getppid() != 1)
	{
		error("zinit-serviced should be run as a child process of PID 1.");
		return 1;
	}

	if (!std::filesystem::exists("/etc/zinit.conf")) { panic("Can't find config file"); }
	std::unordered_map<std::string, std::string> configMap = parseFile("/etc/zinit.conf");
	checkconfigmap(configMap);

	int ssocket_fd;
	if (init_serviced_socket(ssocket_fd) != 0)
	{
		error("Failed to open UNIX socket. Communication with the init process may not be possible.");
	}

	listen(ssocket_fd, 5);
	make_nonblocking(ssocket_fd);
	int epfd = epoll_create1(0);
	struct epoll_event ev = {0};
	ev.events = EPOLLIN;
	ev.data.fd = ssocket_fd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, ssocket_fd, &ev);
	struct epoll_event events[10];

	servicedinfo("Loading services...");

	std::vector<Service> services = load_services();

	GraphResult graphresult = build_and_sort_services(services);
	for (auto &s : graphresult.failed)
	{
		error(std::format("Starting service {} failed.", s));
		if (service_exists(s, services))
		{
			Service* sv = &find_sv_by_name(s, services);
			sv->note = "Start failed due to missing dependency.";
		}
	}
	std::unordered_set<std::string> failed_set(graphresult.failed.begin(), graphresult.failed.end());
	std::unordered_map<std::string, Service*> svc_map;
	for (auto& s : services)
		svc_map[s.name] = &s;

	for (const std::string& service : graphresult.order)
	{
		if (failed_set.contains(service))
			continue;

		auto it = svc_map.find(service);
		if (it == svc_map.end())
			continue;
		Service* s = it->second;

		int status = start_service(*s);
		std::this_thread::sleep_for(std::chrono::milliseconds(std::stol(configMap.at("SV_START_DELAY"))));
		if (status == 1)
		{
			error(std::format("Starting service {} failed.", service));
			s->note = "Start failed.";
		}
	}

	int status;
	while (true)
	{
		int n = epoll_wait(epfd, events, 10, -1);
		for (int i = 0; i < n; i++)
		{
			if (events[i].data.fd == ssocket_fd)
			{
				int client_fd = accept(ssocket_fd, nullptr, nullptr);
				make_nonblocking(client_fd);
				struct epoll_event cev = {0};
				cev.events = EPOLLIN;
				cev.data.fd = client_fd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
			}
			else
			{
				int fd = events[i].data.fd;
				Command sstruct;
				std::string json_sstruct;
				json_sstruct.resize(4096);
				ssize_t r = read(fd, json_sstruct.data(), json_sstruct.size());
				if (r > 0)
					json_sstruct.resize(r);
				else
					json_sstruct.clear();

				Service sv{};
				std::string json_sv;
				std::string response;
				if (r <= 0)
				{
					close(fd);
				}
				else
				{
					sstruct = json_to_command(json_sstruct);
					if (sstruct.cmd == "SD_SVSTART")
					{
						if (!service_exists(sstruct.arg, services))
						{
							response = "Service doesn't exist.\n";
						}
						else
						{
							for (Service& s : services)
							{
								if (sstruct.arg == s.name)
								{
									if (s.pid != -1)
									{
										response = "Service already started.\n";
									}
									else
									{
										int st = start_service(s);
										s.note = "Manually started.";
										if (st == 0)
											response = "Service started successfully.\n";
										else
											response = "Service failed to start: fork() failed.";
									}
									break;
								}
							}
						}
					}
					else if (sstruct.cmd == "SD_SVSTOP")
					{
						if (!service_exists(sstruct.arg, services))
						{
							response = "Service doesn't exist.\n";
						}
						else
						{
							for (Service& s : services)
							{
								if (sstruct.arg == s.name)
								{
									if (s.pid == -1)
									{
										response = "Service already stopped.\n";
									}
									else if (s.type == "oneshot")
									{
										response = "Can't stop oneshot service.\n";
									}
									else
									{
										stop_service(s);
										response = std::format("Service {} stopped.\n", s.name);
									}
								}
							}
						}
					}
					else if (sstruct.cmd == "SD_SVRESTART")
					{
						if (!service_exists(sstruct.arg, services))
						{
							response = "Service doesn't exist.\n";
						}
						else
						{
							for (Service& s : services)
							{
								if (sstruct.arg == s.name)
								{
									if (s.pid == -1)
									{
										response = "Service not started.\n";
									}
									else
									{
										stop_service(s);
										int st = start_service(s);
										if (st == 0)
											response = std::format("Service {} restarted.\n", s.name);
										else
											response = "Service failed to start: fork() failed.";
									}
								}
							}
						}
					}
					else if (sstruct.cmd == "SD_SVSTATUS")
					{
						if (!service_exists(sstruct.arg, services))
						{
							sv.name = "UNKNOWN";
						}
						else
						{
							sv = find_sv_by_name(sstruct.arg, services);
						}
					}
					else if (sstruct.cmd == "INIT_POWEROFF" || sstruct.cmd == "INIT_REBOOT") {
						for (Service& s : services)
						{
							stop_service(s);
							s.note = "Stopped due to a planned shutdown/restart";
						}
						response = std::format("Shutting down...");
						if (sstruct.cmd == "INIT_POWEROFF")
							initreboot();
						else
							inithalt();
					}
					else if (sstruct.cmd == "SD_SVREFRESH")
					{
						refresh_services(services);
						response = "Services successfully refreshed.";
					}
					else
					{
						response = "Unknown command.\n";
					}

					if (sstruct.cmd == "SD_SVSTATUS")
					{
						json_sv = service_to_json(sv);
						write(fd, json_sv.c_str(), json_sv.size());
						close(fd);
					}
					else
					{
						write(fd, response.c_str(), response.size());
						close(fd);
					}
				}
			}
		}

		pid_t pid = waitpid(-1, &status, WNOHANG);

		if (pid > 0)
		{
			for (auto& s : services)
			{
				if (s.pid == pid)
				{
					s.running = false;
					if (s.autorestart)
					{
						if (s.restartcount < 5)
						{
							s.restartcount++;
							start_service(s);
						}
						else
						{
							s.note = "Service failed to start.\n";
							s.pid = -1;
							error(std::format("Service {} failed to start.", s.name));
						}
					}
					else
					{
						s.pid = -1;
						if (WEXITSTATUS(status) != 0)
						{
							error(std::format("Service {} exited with status {}", s.name, WEXITSTATUS(status)));
							s.note = std::format("Service exited with status {}", WEXITSTATUS(status));
						}
						else
						{
							s.note = "Service exited normally.";
						}
					}
				}
			}
		}
	}
	return 1;
}