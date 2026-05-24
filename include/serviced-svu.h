#ifndef SERVICED_SVU_H
#define SERVICED_SVU_H

#include <vector>
#include <string>
#include "serviced.h"

// WARNING: that function doesn't check if service actually exists, so use it with caution as incorrect usage may cause a segfault
Service& find_sv_by_name(const std::string& name, std::vector<Service>& services);
bool service_exists(const std::string& name, const std::vector<Service>& services);

#endif