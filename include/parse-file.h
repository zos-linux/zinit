#ifndef PARSE_FILE_H
#define PARSE_FILE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

std::unordered_map<std::string, std::string> parseFile(std::filesystem::path filename);

std::vector<std::string> csvToVector(const std::string& csv);

#endif