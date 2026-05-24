#ifndef SERVICED_FILE_MANAGEMENT_H
#define SERVICED_FILE_MANAGEMENT_H

#include <string>
#include <unordered_map>
#include "serviced.h"

int checkservicefile(const std::unordered_map<std::string,std::string>& services, Service* service_struct);

#endif