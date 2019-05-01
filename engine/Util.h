#pragma once
#include <windows.h>
#include <stdio.h>

#pragma warning(disable:4996);

#define ERROR_MSG(msg) std::cout << __FUNCDNAME__ << " " << __LINE__ << " Error: " << msg << "\n";
bool VERBOSE = false;

bool IsPrivateAddress(char* ip, bool verbose = false)
{
	char ipcopy[30];
	strcpy(ipcopy, ip);
	const char *a, *b, *c, *d;
	a = strtok(ipcopy, "."), b = strtok(nullptr, "."), c = strtok(nullptr, "."), d = strtok(nullptr, ".");
	if (verbose) 
		printf("checking if %s:%s:%s:%s is private\n", a, b, c, d);

	if (atoi(a) == 10) // 10.0.0.0 to 10.255.255.255
	{
		if (verbose)
			printf("%s.%s.%s.%s is private\n", a, b, c, d);
		return true;
	}
	if (atoi(a) == 172 && atoi(b) >= 16 && atoi(b) <= 31) // 172.16.0.0 to 172.31.255.255
	{
		if (verbose)
			printf("%s.%s.%s.%s is private\n", a, b, c, d);
		return true;
	}
	if (atoi(a) == 192 && atoi(b) == 168)
	{
		if (verbose)
			printf("%s.%s.%s.%s is private\n", a, b, c, d);
		return true;
	}
	if (verbose)
		printf("%s.%s.%s.%s is public\n",a,b,c,d);
	return false;
}

void ErrorExit()
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, "Error", MB_ICONERROR | MB_OK);

	LocalFree(lpMsgBuf);
	ExitProcess(dw);
}

char* GetPcName()
{
	char pcName[100], pcUsername[100];
	DWORD dwPcNameSize = 100, dwPcUsernameSize = 100;
	if (!GetUserName(pcUsername, &dwPcUsernameSize))
	{
		ERROR_MSG("could not get pc user name\n");
		ErrorExit();
	}
	if (!GetComputerName(pcName, &dwPcNameSize))
	{
		ERROR_MSG("could not get pc name\n");
		ErrorExit();
	}
	char *pcFullName = new char[strlen(pcName) + strlen(pcUsername) + 1];
	strcpy(pcFullName, pcName);
	strcat(pcFullName, "-");
	strcat(pcFullName, pcUsername);
	return pcFullName;
}
