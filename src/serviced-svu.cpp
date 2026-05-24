#include <string>
#include "serviced.h"
#include <vector>
#include "events.h"

Service& find_sv_by_name(const std::string& name, std::vector<Service>& services)
{
	for (auto& service : services)
	{
		if (service.name == name)
			return service;
	}

	debuginfo("find_sv_by_name() terminated: service doesn't exist");
	std::abort();
}

bool service_exists(const std::string& name, const std::vector<Service>& services)
{
	bool exists = false;
	for (const auto& service : services)
	{
		if (service.name == name)
			exists = true;
	}
	return exists;
}