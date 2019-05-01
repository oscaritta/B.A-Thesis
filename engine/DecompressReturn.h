#pragma once

struct DecompressReturn
{
	int Width;
	int Height;
	int Left;
	int Top;
	unsigned char* data;
};