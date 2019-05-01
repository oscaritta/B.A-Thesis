#pragma once

#include <fstream>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <map>

class PrefsParser
{
private:
	std::ifstream fin;
	std::map<std::string, std::string> preferences;
public:
	PrefsParser(std::string strFilename)
	{
		fin.open(strFilename);
		std::string line;
		do
		{
			std::getline(fin, line);
			std::istringstream stream(line);
			std::string key, value;
			stream >> key >> value;
			preferences.insert(std::pair<std::string, std::string>(key, value));
		} while (!fin.eof());
	}
	std::string operator[](std::string key)
	{
		return preferences[key];
	}

};