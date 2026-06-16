#include <iostream>
#include "rang.hpp"
#include "osdetector.h"
#include <string>
#include "events.h"
#include <unordered_map>
#include <pty.h>
#include "parse-file.h"
#include <unistd.h>
#include <nlohmann/json_fwd.hpp>
#include <sys/mount.h>
#include <sys/stat.h>
#include "sserver/init-socket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include "serviced.h"
#include "csignal"
using namespace rang;

volatile int sigchld;

static void sigchld_handler(int sig)
{
    (void)sig;
    sigchld = 1;
}

bool is_mounted(const char* path)
{
    struct stat st, parent;

    if (stat(path, &st) != 0)
        return false;

    std::string parent_path = std::string(path) + "/..";

    if (stat(parent_path.c_str(), &parent) != 0)
        return false;

    return st.st_dev != parent.st_dev;
}

void mountdevfs()
{
    namespace fs = std::filesystem;
    if (!fs::exists("/dev"))
    {
        mkdir("/dev", 0755);
    }
    info("Mounting devtmpfs...");
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, "") != 0)
    {
        panic(std::format("Can't mount devtmpfs on /dev: {}", strerror(errno)));
    }

    if (!fs::exists("/dev/pts"))
    {
        mkdir("/dev/pts", 0755);
    }
    if (mount("devpts", "/dev/pts", "devpts", 0, "newinstance,ptmxmode=0666,mode=0620") != 0)
    {
        panic(std::format("Can't mount devpts on /dev/pts: {}", strerror(errno)));
    }

    if (!(fs::exists("/dev/ptmx") && fs::is_symlink("/dev/ptmx")))
    {
        symlink("/dev/pts/ptmx", "/dev/ptmx");
    }
}

bool mountsys()
{
    namespace fs = std::filesystem;
    const fs::path mnt = "/sys";
    info("Mounting sysfs on /sys");
    if (!fs::exists(mnt))
    {
        if (mkdir(mnt.c_str(), 0755) != 0 )
        {
            error(std::format("Can't mount sysfs on /sys: {}", strerror(errno))); return false;
        }
    }

    if (mount("sysfs", mnt.c_str(), "sysfs", 0, nullptr) != 0) { error(std::format("Can't mount sysfs on /sys: {}", strerror(errno))); return false; }
    return true;
}

bool is_uefi()
{
    return std::filesystem::exists("/sys/firmware/efi");
}

void mountefivarfs()
{
    namespace fs = std::filesystem;
    const fs::path mnt = "/sys/firmware/efi/efivars";
    if (!is_uefi())
        return;
    else
        info("The system is running in UEFI mode. Mounting the efivarfs at /sys/firmware/efi/efivars");

    if (!fs::exists(mnt))
    {
        mkdir(mnt.c_str(), 0755);
    }

    if (mount("efivarfs", mnt.c_str(), "efivarfs", MS_RDONLY, nullptr) != 0)
    {
        error(std::format("Can't mount efivarfs on /sys/firmware/efi/efivars: {}", strerror(errno)));
    }
}

void mountrun()
{
    namespace fs = std::filesystem;
    if (!fs::exists("/run"))
    {
        mkdir("/run", 0755);
    }
    info ("Mounting tmpfs on /run");
    if (mount("tmpfs", "/run", "tmpfs", 0, "rw") != 0)
    {
        panic(std::format("Can't mount tmpfs on /run: {}", strerror(errno)));
    }
    mkdir("/run/zinit", 0755);
}

void mountproc()
{
    namespace fs = std::filesystem;
    info("Mounting proc on /proc");
    if (!fs::exists("/sys"))
    {
        if (mkdir("/proc", 0755) != 0)
        {
            error(std::format("Can't mount proc on /proc: {}", strerror(errno))); return;
        }
    }
    if (mount("proc", "/proc", "proc", 0, nullptr) != 0)
    {
        error(std::format("Can't mount sysfs on /sys: {}", strerror(errno)));
    }
}

void checkpid()
{
    if (getpid() != 1)
    {
        error("Must be run as PID 1.");
        exit(1);
    }
}

bool isvalidservicedcommand(const std::string& command)
{
    std::vector<std::string> valid = {"SD_SVSTART", "SD_SVSTOP", "SD_SVRESTART", "SD_SVSTATUS", "SD_SVREFRESH"};
    for (const std::string& cmd : valid)
    {
        if (cmd == command) return true;
    }
    return false;
}

int main()
{
    checkpid();
    signal(SIGCHLD, sigchld_handler);
    std::cout << fg::green << "Welcome to " << fg::blue << detectos() << "!" << fg::reset << std::endl << std::endl;

    if (!is_mounted("/dev"))
        mountdevfs();
    if (!is_mounted("/run"))
        mountrun();
    if (!is_mounted("/sys"))
        if (mountsys())
            mountefivarfs();
    if (!is_mounted("/proc"))
        mountproc();

    if (!std::filesystem::exists("/etc/zinit.conf")) { panic("Can't find config file"); }
    std::unordered_map<std::string, std::string> configMap = parseFile("/etc/zinit.conf");
    if (!configMap.contains("SERVICED_PATH"))
    {
        configMap["SERVICED_PATH"] = "/usr/sbin/zinit-serviced";
    }

    if (std::filesystem::exists(configMap.at("SERVICED_PATH")) && access(configMap.at("SERVICED_PATH").c_str(), X_OK) == 0)
    {
        info("Starting zinit-serviced...");
        pid_t serviced_pid = fork();
        if (serviced_pid == 0)
        {
            char *serviced_argv[] = {"zinit-serviced", nullptr};
            execve(configMap.at("SERVICED_PATH").c_str(), serviced_argv, nullptr);
            panic("zinit-serviced exited");
            _exit(1);
        }
    }
    else
    {
        panic("Can't start zinit-serviced");
    }

    int socket_fd;
    bool socketinit = true;
    if (init_main_socket(socket_fd) != 0)
    {
        error("Failed to open UNIX socket. Communication with the serviced process may not be possible.");
        socketinit = false;
    }
    while (true)
    {
        if (socketinit)
        {
            int client_fd = accept(socket_fd, nullptr, nullptr);
            if (client_fd < 0)
            {
                error(strerror(errno));
                continue;
            }

            std::string response;
            ucred client_credentials;
            socklen_t len = sizeof(client_credentials);
            if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &client_credentials, &len) < 0)
            {
                response = std::format("Can't get peer credentials: {}\n", strerror(errno));
                write(client_fd, response.c_str(), response.size());
                close(client_fd);
                continue;
            }
            else
            {
                if (client_credentials.uid != 0)
                {
                    response = "Permission denied\n";
                    write(client_fd, response.c_str(), response.size());
                    close(client_fd);
                    continue;
                }
            }

            Command command;
            std::string json_command;
            json_command.resize(4096);
            int n = read(client_fd, json_command.data(), json_command.size());
            if (n > 0)
                json_command.resize(n);
            else
                json_command.clear();

            std::string sd_response;
            sd_response.resize(4096);
            std::string json_sd_service;
            json_sd_service.resize(4096);
            if (n > 0)
            {
                command = json_to_command(json_command);
                int serviced_fd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (serviced_fd < 0)
                {
                    response = std::format("Can't open serviced socket: {}", strerror(errno));
                    write(client_fd, response.c_str(), response.size());
                    close(client_fd);
                    continue;
                }
                sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                strcpy(addr.sun_path, "/run/zinit/serviced.sock");
                if (connect(serviced_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
                {
                    response = std::format("Could not connect to serviced socket: {}", strerror(errno));
                    write(client_fd, response.c_str(), response.size());
                    close(serviced_fd);
                    close(client_fd);
                    continue;
                }

                if (command.cmd == "INIT_POWEROFF")
                {
                    write(serviced_fd, json_command.c_str(), json_command.size());
                    read(serviced_fd, nullptr, 0);
                    close(serviced_fd);

                    constexpr char msg[64] = "The system will now shut down.\n";
                    write(client_fd, msg, sizeof(msg));
                    inithalt();
                }
                else if (command.cmd == "INIT_REBOOT")
                {
                    write(serviced_fd, json_command.c_str(), json_command.size());
                    read(serviced_fd, nullptr, 0);
                    close(serviced_fd);

                    constexpr char msg[64] = "The system will now reboot.\n";
                    write(client_fd, msg, sizeof(msg));
                    close(client_fd);
                    initreboot();
                }
                else if (isvalidservicedcommand(command.cmd))
                {
                    write(serviced_fd, json_command.c_str(), json_command.size());
                    if (command.cmd == "SD_SVSTATUS")
                        read(serviced_fd, json_sd_service.data(), json_sd_service.size());
                    else
                        read(serviced_fd, sd_response.data(), sd_response.size());
                }
                close(serviced_fd);

                if (command.cmd == "SD_SVSTATUS")
                    write(client_fd, json_sd_service.c_str(), json_sd_service.size());
                else
                    write(client_fd, sd_response.c_str(), sd_response.size());
                close(client_fd);
            }
            else
            {
                close(client_fd);
            }
        }
        if (sigchld == 1)
        {
            sigchld = 0;
            int pid;
            int status;
            while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {}
        }
    }
}