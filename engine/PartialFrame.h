#pragma once
#include <string>

struct PartialFrame
{
	std::string filename;
	int left;
	int top;
	unsigned char* compressed;
	int size;
	int checksum;
	
	PartialFrame(std::string filename, int left, int top)
	{
		this->filename = filename;
		this->left = left;
		this->top = top;
	}
	PartialFrame(unsigned char* compressed, int size, int left, int top, int checksum)
	{
		this->size = size;
		this->left = left;
		this->top = top;
		this->compressed = compressed;
		this->checksum = checksum;
	}
};