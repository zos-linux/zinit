#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include "rang.hpp"
#include <unordered_map>
#include <filesystem>
#include <sstream>
using namespace rang;

std::string trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");

    if (start == std::string::npos)
        return "";

    return s.substr(start, end - start + 1);
}

bool is_ignorable(const std::string& line)
{
	auto is_ws = [](unsigned char c) { return std::isspace(c); };

	auto it = std::find_if_not(line.begin(), line.end(), is_ws);

	if (it == line.end())
		return true;

	return (*it == '#' || *it == ';');
}

std::unordered_map<std::string, std::string> parseFile(std::filesystem::path filename)
{
    std::ifstream file(filename);
    if (!file.good())
    {
        std::cerr << fg::red << "Error: " << fg::blue << "Could not open file " << filename << fg::reset << std::endl;
        exit(1);
    }

    std::vector<std::string> fileLines;
    std::string line;
    while (std::getline(file, line))
    {
        fileLines.push_back(line);
    }
    file.close();

	std::unordered_map<std::string, std::string> map;

	for (const auto& l : fileLines)
	{
		if (is_ignorable(l))
			continue;

		auto pos = l.find("=");
		if (pos == std::string::npos)
			continue;

		std::string key = trim(l.substr(0, pos));
		std::string value = trim(l.substr(pos + 1));

		if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
		{
			value = value.substr(1, value.size() - 2);
		}

		map[key] = value;
	}

	if (!map.contains("service_name"))
		map["service_name"] = filename.filename();
	return map;
}

std::vector<std::string> csvToVector(const std::string& csv)
{
	std::vector<std::string> result;
	std::stringstream ss(csv);
	std::string line;

	while (std::getline(ss, line, ','))
	{
		result.push_back(trim(line));
	}
	return result;
}
