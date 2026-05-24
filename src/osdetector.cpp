#include <fstream>
#include <iostream>
#include <string>

std::string detectos()
{
	std::ifstream file("/etc/os-release");
	if (!file.good())
	{
		return "Unknown Linux";
	}

	std::string line;

	while (std::getline(file, line))
	{
		if (line.rfind("PRETTY_NAME=", 0) == 0)
		{
			std::string value = line.substr(13);


			if (!value.empty() && value.front() == '"')
				value.erase(0, 1);
			if (!value.empty() && value.back() == '"')
				value.pop_back();

			return value;
		}
	}
	return "Unknown OS";
}