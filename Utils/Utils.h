#pragma once

#include <string>
#include <string.h>
#include <algorithm>
#include <vector>

namespace Utils
{
	std::string to_lower(std::string str);
	std::vector<std::string> split_str(std::string str, const std::string& delimiter);
}