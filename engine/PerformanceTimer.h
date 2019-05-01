#pragma once

#include <windows.h>

class PerformanceTimer
{
private:
	double m_PCFreq;
	static PerformanceTimer *timer;

	PerformanceTimer()
	{
		LARGE_INTEGER li;
		if (!QueryPerformanceFrequency(&li))
			printf("QueryPerformanceFrequency failed!\n");
		m_PCFreq = double(li.QuadPart) / 1000.0;
	}

	double _GetTicks()
	{
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		return double(li.QuadPart) / m_PCFreq;
	}

public:
	static double GetTicks();
};

PerformanceTimer* PerformanceTimer::timer = nullptr;

double PerformanceTimer::GetTicks()
{
	if (timer == nullptr)
		timer = new PerformanceTimer();
	return timer->_GetTicks();
}