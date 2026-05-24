#include "events.h"
#include <format>
#include <string>
#include <vector>
#include <unordered_map>
#include "parse-file.h"
#include "serviced.h"

int checkservicefile(const std::unordered_map<std::string,std::string>& services, Service* service_struct)
{
	if (services.contains("type"))
	{
		if (services.at("type") != "oneshot" && services.at("type") != "process")
		{
			error(std::format("Unknown service type: {}", services.at("type")));
			return 1;
		}
		else
		{
			if (services.at("type") == "oneshot")
				service_struct->autorestart = false;
			else
				service_struct->autorestart = true;

			service_struct->type = services.at("type");
		}
	}
	else
	{
		error(std::format("Service {} doesn't contain type", services.at("service_name")));
		return 1;
	}

	if (!services.contains("command"))
	{
		error(std::format("Service {} doesn't contain command", services.at("service_name")));
		return 1;
	}

	if (services.contains("dependencies"))
	{
		service_struct->deps = csvToVector(services.at("dependencies"));
	}
	else
	{
		service_struct->deps.push_back("none");
	}
	if (services.contains("setsid"))
	{
		if (services.at("setsid") == "true")
			service_struct->setsid = true;
		else if (services.at("setsid") == "false")
			service_struct->setsid = false;
		else
		{
			warn(std::format("Invalid setsid argument in the {} service, falling back to false", services.at("service_name")));
			return 1;
		}
	}
	else
	service_struct->command = services.at("command");
	service_struct->name = services.at("service_name");
	return 0;
}