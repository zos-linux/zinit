#ifndef INFO_H
#define INFO_H
#include <iostream>
#include "rang.hpp"
using namespace rang;

void printVersion() { std::cout << fg::blue << "systemctl from zinit 1.0.0" << fg::reset << std::endl; }

#endif