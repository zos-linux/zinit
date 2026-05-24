#ifndef EVENTS_H
#define EVENTS_H
#include <string>

void spawn_emergencyshell();
void panic(const std::string& msg);
void info(const std::string& msg);
void error(const std::string& msg);
void servicedinfo(const std::string& msg);
void warn(const std::string& msg);
void debuginfo(const std::string& msg);

void initreboot();
void inithalt();
#endif