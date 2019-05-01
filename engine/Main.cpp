// include the basic windows header files and the Direct3D header file
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <WinSock2.h>
#include <windowsx.h>
#include <d3d9.h>
#include <Wincodec.h>  
#include <shlobj.h>
#include <shellapi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <gl/gl.h>
#include <math.h>
#include <stdlib.h>
#include <fstream>
#include <memory>
#include <algorithm>
#include <string>
#include <iostream>
#include <future>
#include <time.h>
#include <queue>
#include <turbojpeg.h>

#include "Util.h"
#include "CComPtrCustom.h"
#include "DirectXHelper.h"
#include "PrefsParser.h"
#include "Networking.h"
#include "PerformanceTimer.h"
#include "Event.h"
#include "PartialFrame.h"
#include "DecompressReturn.h"
#include "CompressReturn.h"
#include "OutputManager.h"

#pragma warning(disable:4996)
#pragma comment (lib, "d3d9.lib")
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "Ws2_32.lib")

#define SLEEP_AFTER_FRAME_RENDER 200
#define MAX_SOCKET_BUFFER 10000000
#define SCREEN_WIDTH 1600
#define SCREEN_HEIGHT 1200
#define CURRENT_TEXTURE_ID 1
#define PREVIOUS_TEXTURE_ID 1
#define FULL_PAINT_THRESHOLD 30.0 
#define DIFFERENCE(a, b) a != b ? distinct=true,a : 0;
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)
#define HRCHECK(__expr) {hr=(__expr);if(FAILED(hr)){wprintf(L"FAILURE 0x%08X (%i)\n\tline: %u file: '%s'\n\texpr: '" WIDEN(#__expr) L"'\n",hr, hr, __LINE__,__WFILE__);goto cleanup;}}
#define RELEASE(__p) {if(__p!=nullptr){__p->Release();__p=nullptr;}}
#define MAX_THRESHOLD_SEND_BITMAP_CHANGED_REGIONS 50.0//if this threshold is not "touched" then send the changed square with its coordinates as png
#define MAX_FILES_TO_SEND 20
#define MAX_RECTS 1000
#define UDP_TEST_PACKET_DELAY 100

std::thread*						copyThreads[100];
HWND								hWnd;
BYTE*								shot = nullptr;
BYTE*								partial = nullptr;
BYTE*								prevShot = nullptr;
BYTE*								difference = nullptr;
BYTE*								pngBuffer = nullptr;
HRESULT								hr = S_OK;
IDirect3D9*							d3d = nullptr;
IDirect3DDevice9*					device = nullptr;
IDirect3DSurface9*					surface = nullptr;
OutputManager*						output = nullptr;
D3DPRESENT_PARAMETERS				parameters = { 0 };
D3DDISPLAYMODE						mode;
D3DLOCKED_RECT						rc;
DXHELP::DirectXHelper*				dxHelper = nullptr;
UINT								pitch;
SYSTEMTIME							st;
int									g_Distinct = 0;
int									g_Mini, g_Minj, g_Maxi, g_Maxj;
int									g_Width = 1;
int									g_Height = 1;
int									RemoteWidth = 0;
int									RemoteHeight = 0;
bool								GL_Init_Done = false;
char								g_LogMessage[500];
std::ofstream						stats("stats.txt");
PrefsParser*						prefs = new PrefsParser("config.ini");
Networking*							pNetworking;
int									MaxSize;
bool								bNewFrame = false;
bool								bUDPReady = false;
std::queue<Event>					Events;
std::queue<IWICBitmap*>				FrameQueue;
int									Broadcaster = -1;
char								RemoteId[20];
char								RemotePassword[20];
int									ImageQuality = 20;

char*								sessionType = nullptr;
char*								partnerIpLocal = nullptr;
char*								partnerIpRemote = nullptr;
char*								partnerPortLocal = nullptr;
char*								partnerPortRemote = nullptr;
char*								partnerUDPPortLocal = nullptr;
char*								partnerUDPPortRemote = nullptr;
char*								listenPort = nullptr;
char								pcPartnerName[201];

inline HRESULT						Direct3D11TakeScreenshot(int& bytesWrittenToFile, std::vector<PartialFrame>& partialFrames, bool& fullFrame);
void								InitConsole();
HWND								CreateOpenGLWindow(const char* title, int x, int y, int width, int height, BYTE type, DWORD flags);
HRESULT								SavePixelsToFile32bppPBGRA(UINT width, UINT height, UINT stride, LPBYTE pixels, int bufsize, const GUID& format);
int									SavePixelsToJpg(UINT width, UINT height, LPBYTE pixels, const char* filePath);
CompressReturn						SavePixelsToJpgMemory(UINT width, UINT height, LPBYTE pixels, int left, int top);
LRESULT	CALLBACK					WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
HRESULT								DecodeImageFromBuffer(BYTE* inBuffer, int inSize, BYTE* outBuffer, int outSize, int& width, int& height, WICPixelFormatGUID& Format);
int									DecodeJpg(BYTE* _compressedImage, int _jpegSize, BYTE* outBuffer, int& Width, int& Height);
DecompressReturn					DecompressThreadFunc(unsigned char* _compressedImage, int _jpegSize, int left, int top);
int									checksum_mod_1m(unsigned char* data, int size);
int									ReceiveImages(const char* LocalIpServer, const char* RemoteIpServer, const char* LocalPortServer, const char* RemotePortServer,
												  const char* LocalPortUDPPartner, const char* RemotePortUDPPartner, const char* ListenPort);
int									StartGUI();
BOOL								SendReceiveInput(const char* IpPartner, const char* PortPartner, const char* ListenPort);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	if ((*prefs)["cli"] == "1")
		InitConsole();
	VERBOSE = (*prefs)["verbose"] == "1";

	printf("Command line: %s\n", lpCmdLine);
	sessionType = strtok(lpCmdLine, " ");
	partnerIpLocal = strtok(nullptr, " ");
	partnerIpRemote = strtok(nullptr, " ");
	partnerPortLocal = strtok(nullptr, " ");
	partnerPortRemote = strtok(nullptr, " ");
	partnerUDPPortLocal = strtok(nullptr, " ");
	partnerUDPPortRemote = strtok(nullptr, " ");
	listenPort = strtok(nullptr, " ");
	if (partnerUDPPortLocal == nullptr || partnerUDPPortRemote == nullptr || sessionType == nullptr || partnerIpLocal == nullptr || partnerIpRemote == nullptr || partnerPortLocal == nullptr || partnerPortRemote == nullptr)
	{
		MessageBox(nullptr, "Invalid Arguments", "Arguments Error", MB_ICONERROR | MB_OK);
		return 1;
	}
	printf("Session type: %s\n", sessionType);
	printf("Partner Local Addr TCP: %s:%s\n", partnerIpLocal, partnerPortLocal);
	printf("Partner Global Addr TCP: %s:%s\n", partnerIpRemote, partnerPortRemote);
	printf("Partner Local Addr UDP: %s:%s\n", partnerIpLocal, partnerUDPPortLocal);
	printf("Partner Global Addr UDP: %s:%s\n", partnerIpRemote, partnerUDPPortRemote);

	Broadcaster = !strcmp(sessionType, "server");

	// Block until we know if we send or receive images
	while (Broadcaster == -1);

	//It's important to instantiate it here so that there are no calls to send/recv/bind/connect/listen before WSAStartup()
	pNetworking = Networking::getInstance(partnerIpLocal, partnerIpRemote, std::string(listenPort));

	if (! Broadcaster)
	{
		std::thread *gui = new std::thread(StartGUI);
		ReceiveImages(partnerIpLocal, partnerIpRemote, partnerPortLocal, partnerPortRemote, partnerUDPPortLocal, partnerUDPPortRemote, listenPort);
	}
	else
	{
		printf("Broadcasting mode: on\n");
		dxHelper = new DXHELP::DirectXHelper;
		if (FAILED(dxHelper->initD3D())) { printf("304 failed\n"); }
		if (FAILED(dxHelper->getDXGIAdapter())) { printf("308 failed\n"); }
		if (FAILED(dxHelper->getDXGIOutput())) { printf("312 failed\n"); }
		if (FAILED(dxHelper->createTextures())) { printf("316 failed\n"); }

		// allocate screenshot buffer
		shot = new BYTE[dxHelper->lOutputDuplDesc.ModeDesc.Width * dxHelper->lOutputDuplDesc.ModeDesc.Height * 4];
		partial = new BYTE[dxHelper->lOutputDuplDesc.ModeDesc.Width * dxHelper->lOutputDuplDesc.ModeDesc.Height * 4];
		prevShot = new BYTE[dxHelper->lOutputDuplDesc.ModeDesc.Width * dxHelper->lOutputDuplDesc.ModeDesc.Height * 4];
		difference = new BYTE[dxHelper->lOutputDuplDesc.ModeDesc.Width * dxHelper->lOutputDuplDesc.ModeDesc.Height * 4];
		pngBuffer = new BYTE[dxHelper->lOutputDuplDesc.ModeDesc.Width * dxHelper->lOutputDuplDesc.ModeDesc.Height * 4];


		static double FirstFrameTime = PerformanceTimer::GetTicks();
		static int	  TotalFrames = 0;

		char* connectedIp = nullptr;
		char* connectedPort = nullptr;
		if (pNetworking->WaitForClient(partnerIpLocal, partnerIpRemote, partnerPortLocal, partnerPortRemote, &connectedIp))
		{
			ERROR_MSG("WaitForClient() returned positive");
			ErrorExit();
			return 1;
		}

		if (connectedIp == nullptr)
		{
			ERROR_MSG("Connected Ip is null\n");
			return 1;
		}
		
		printf("%s is our peer now. UDP packets will be sent to this address", connectedIp);
		if (!strcmp(connectedIp, partnerIpLocal))
			connectedPort = partnerUDPPortLocal;
		if (!strcmp(connectedIp, partnerIpRemote))
			connectedPort = partnerUDPPortRemote;

		if (connectedPort == nullptr)
		{
			ERROR_MSG("Connected Port is null <=> could not find partner ip equal to our peer\n");
			return 1;
		}

		printf("%s:%s is our peer now. UDP packets will be sent to this address", connectedIp, connectedPort);

		std::thread inputSendReceiveThread(SendReceiveInput, connectedIp, connectedPort, listenPort);

		while (!bUDPReady);
		printf("UDP Ready\n");

		/* Get PC Username */
		char *pcFullName = GetPcName();

		/* Main Loop */
		do
		{
			char	buffer[1024];
			int		maxlen = 1023;
			int		received;

			received = pNetworking->ReceiveFromClient(buffer, maxlen);
			//received = pNetworking->ReceiveFromServer(buffer, maxlen);
			if (received > 0)
			{
				buffer[received] = 0;
				printf("received: %s\n", buffer);

				std::string strBuffer(buffer);

				if (strBuffer == std::string("get_pc_name"))
				{
					int sent = pNetworking->SendToClient(pcFullName, strlen(pcFullName));
					if (sent <= 0)
					{
						ERROR_MSG("could not send pc name(" << pcFullName << ")\n");
						ErrorExit();
					}
				}
				if (strBuffer == std::string("save_my_name"))
				{
					received = pNetworking->ReceiveFromClient(pcPartnerName, 200);
					if (received < 0)
					{
						ERROR_MSG("could not get remote pc name\n");
						ErrorExit();
					}
					pcPartnerName[received] = 0;

					int sent = pNetworking->SendToClient((char*)"ok", 2);
					if (received < 0)
					{
						ERROR_MSG("could not send 'ok'\n");
						ErrorExit();
					}

				}
				if (strBuffer == std::string("get_screen_width"))
				{
					int Width = dxHelper->lOutputDuplDesc.ModeDesc.Width;
					//printf << "let's give the client the width: " << Width << std::endl;
					std::string resp = std::to_string(Width);
					while (resp.size() < 5)
						resp += " ";
					int sent = pNetworking->SendToClient((char*)resp.c_str(), 5);
					//int sent = pNetworking->SendToServer((char*)resp.c_str(), 5);
					if (sent > 0)
					{
						//printf("bytes sent: %d vs bytes written to file: %d\n", sent, bytesWrittenToFile);
					}
					else
					{
						ERROR_MSG("sent failed: " << sent);
						ErrorExit();
					}
				}
				if (strBuffer == std::string("get_screen_height"))
				{
					int Height = dxHelper->lOutputDuplDesc.ModeDesc.Height;
					//printf << "let's give the client the height: " << Height << std::endl;
					std::string resp = std::to_string(Height);
					while (resp.size() < 5)
						resp += " ";
					int sent = pNetworking->SendToClient((char*)resp.c_str(), 5);
					//int sent = pNetworking->SendToServer((char*)resp.c_str(), 5);
					if (sent > 0)
					{
						//printf("bytes sent: %d vs bytes written to file: %d\n", sent, bytesWrittenToFile);
					}
					else
					{
						ERROR_MSG("sent failed: " << sent);
						ErrorExit();
					}
				}
				if (strBuffer == std::string("get_frame_data"))
				{
					std::vector<PartialFrame> partialFrames;
					bool fullFrame;
					int bytesWrittenToFile;
					double before;
					double after;
					static int traffic = 0;
					static int PartialFrames = 0;

					double totaltotalbefore = PerformanceTimer::GetTicks();
					before = PerformanceTimer::GetTicks();

					do
					{
						Direct3D11TakeScreenshot(bytesWrittenToFile, partialFrames, fullFrame);
						if (partialFrames.size() == 0 && !fullFrame);
							//Beep(2000, 100);
					} while (partialFrames.size() == 0 && !fullFrame);
					if (fullFrame);
						//Beep(1000, 100);
					else;
						//Beep(500, 100);

					if (VERBOSE) printf("\n");
					//printf << std::endl << bytesWrittenToFile / 1000 << " kb" << std::endl;
					after = PerformanceTimer::GetTicks();
					if (VERBOSE) printf("Total time to capture the frame: %f ms\n", after - before);
					if (VERBOSE) printf("-------------\n");

					if (!fullFrame && partialFrames.size() > 0)
					{
						DWORD bb = GetTickCount();

						for (PartialFrame frame : partialFrames)
						{
							/*
							std::string file = frame.filename;
							printf << file << ", ";

							FILE* f = fopen(frame.filename.c_str(), "rb");
							fseek(f, 0, SEEK_END);
							int size = ftell(f);
							fseek(f, 0, SEEK_SET);
							fread(pngBuffer, size, 1, f);
							bytesWrittenToFile = size;
							fclose(f);
							*/
							
							std::string header = (std::string("partialframe;")+std::to_string(frame.left)+
												std::string(";")+
												std::to_string(frame.top)+
												std::string(";")+
												std::to_string(partialFrames.size())+
												std::string(";") + 
												std::to_string(frame.checksum)
												).c_str();
							

							if (VERBOSE) printf("Sending header: %s\n", header.c_str());
							int sent = pNetworking->SendToClient((char*)header.c_str(), strlen(header.c_str()));
							//int sent = pNetworking->SendToServer((char*)header.c_str(), strlen(header.c_str()));
							if (sent > 0)
							{
							}
							else
							{
								ERROR_MSG("sent failed: " << sent);
								ErrorExit();
							}

							if (VERBOSE) printf("Sending %d kb to the receiver\n", frame.size / 1000);
							sent = pNetworking->SendToClient((char*)frame.compressed, frame.size);
							//sent = pNetworking->SendToServer((char*)frame.compressed, frame.size);
							if (sent > 0)
							{
								printf("%d kb sent to the receiver\n", frame.size/1000);
								//printf("bytes sent: %d vs bytes written to file: %d\n", sent, bytesWrittenToFile);
							}
							else
							{
								ERROR_MSG("sent failed: " << sent);
								ErrorExit();
							}
							tjFree(frame.compressed);
							traffic += sent;

						}

						partialFrames.clear();

						PartialFrames++;
						DWORD aa = GetTickCount();
						if (VERBOSE) printf("partial frames sent in %d ms\n", aa - bb);
					}

					else
					{
						if (VERBOSE) printf("sending full frame\n");
						before = PerformanceTimer::GetTicks();
						const char* header = "fullframe";
						int sent = pNetworking->SendToClient((char*)header, strlen(header));
						//int sent = pNetworking->SendToServer((char*)header, strlen(header));
						if (sent > 0)
						{
						}
						else
						{
							ERROR_MSG("sent failed: " << sent);
							ErrorExit();
						}

						sent = pNetworking->SendToClient((char*)pngBuffer, bytesWrittenToFile);
						//sent = pNetworking->SendToServer((char*)pngBuffer, bytesWrittenToFile);
						if (sent > 0)
						{
							printf("bytes sent: %d vs bytes written to file: %d\n", sent, bytesWrittenToFile);
						}

						else
						{
							ERROR_MSG("sent failed: " << sent);
							ErrorExit();
						}
						traffic += sent;
						after = PerformanceTimer::GetTicks();
						//printf("Total time to send the frame: %f ms\n", after - before);
					}
					

					TotalFrames++;
					double LastFrameTime = PerformanceTimer::GetTicks();
					//printf << "Overall FPS: " << TotalFrames / (LastFrameTime - FirstFrameTime) * 1000.0 << std::endl;
					double totaltotalafter = PerformanceTimer::GetTicks();
					//printf << "TotalTotal ms = " << totaltotalafter - totaltotalbefore << std::endl;
					//if (totaltotalafter - totaltotalbefore > 300)
					//	MessageBeep(0xffffffff);
					if (VERBOSE) printf("Average KB/Frame = %d\n", traffic / TotalFrames / 1000);
					if (VERBOSE) printf("Total Frames: %d\n", TotalFrames);
					if (VERBOSE) printf("Partial/Total Frames ratio = %f\n", (float)PartialFrames / (float)TotalFrames);
				}
	
			}
			else
			{
				ERROR_MSG("recv failed: " << received);
				ErrorExit();
			}


		} while (true);

	}

	while (!GetAsyncKeyState(VK_ESCAPE));
	return 0;
}

void InitConsole()
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen("CON", "w", stdout);
	//freopen("log.txt", "w", stdout);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		ExitProcess(0);
		return 0;
	} break;
	
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
		if (GetActiveWindow() == hWnd)
		{
			if (!Broadcaster)
			{
				RECT r;
				GetClientRect(hWnd, &r);
				Events.push(Event(message, wParam, MAKELPARAM(LOWORD(lParam)*1000/(r.right-r.left),HIWORD(lParam)*1000/(r.bottom-r.top))));
			}
		}
		break;
	
	case WM_SIZE:
		int width = LOWORD(lParam);
		int height = HIWORD(lParam);
		glViewport(0, 0, width, height);
		g_Width = width;
		g_Height = height;
		break;
	}
	
	return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND CreateOpenGLWindow(const char* title, int x, int y, int width, int height, BYTE type, DWORD flags)
{
	//int         pf;
	//HDC         hDC;
	HWND        hWnd;
	//PIXELFORMATDESCRIPTOR pfd;
	static HINSTANCE hInstance = 0;

	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = "WindowClass";
	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, "RegisterClassEx() failed:  "
			"Cannot register window class.", "Error", MB_OK);
		return NULL;
	}

	hWnd = CreateWindow("WindowClass", title, WS_OVERLAPPEDWINDOW |
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		x, y, width, height, NULL, NULL, hInstance, NULL);

	if (hWnd == NULL) {
		MessageBox(NULL, "CreateWindow() failed:  Cannot create a window.",
			"Error", MB_OK);
		return NULL;
	}

	/*
	hDC = GetDC(hWnd);

	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | flags;
	pfd.iPixelType = type;
	pfd.cColorBits = 32;

	pf = ChoosePixelFormat(hDC, &pfd);
	if (pf == 0) {
		MessageBox(NULL, "ChoosePixelFormat() failed:  "
			"Cannot find a suitable pixel format.", "Error", MB_OK);
		return 0;
	}

	if (SetPixelFormat(hDC, pf, &pfd) == FALSE) {
		MessageBox(NULL, "SetPixelFormat() failed:  "
			"Cannot set format specified.", "Error", MB_OK);
		return 0;
	}
	*/

	//DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
	//ReleaseDC(hWnd, hDC);
	return hWnd;
}

int StartGUI()
{
	while (RemoteWidth == 0 || RemoteHeight == 0);

	MSG msg;
	hWnd = CreateOpenGLWindow("Remote Desktop Receiver", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, PFD_TYPE_RGBA, PFD_DOUBLEBUFFER);
	ShowWindow(hWnd, SW_SHOW);
	//HDC hDC = GetDC(hWnd);
	//HGLRC hRC = wglCreateContext(hDC);
	//wglMakeCurrent(hDC, hRC);

	output = new OutputManager();
	output->Initialize(hWnd);

	std::mutex *m = new std::mutex;
	std::thread cleaner([](std::mutex *m) {
		do
		{
			while (FrameQueue.size() > 1)
			{
				m->lock();
				FrameQueue.front()->Release();
				FrameQueue.pop();
				m->unlock();
			}
		} while (true);
	}, m);

	while (TRUE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			static double FirstFrameTime = PerformanceTimer::GetTicks();
			static int TotalFrames = 0;

			if (msg.message == WM_QUIT) break;
			if (bNewFrame)
			{
				//RenderFrame();
				//SwapBuffers(hDC);
				if (FrameQueue.size() > 0)
				//if (false)
				{
					m->lock();
					output->BeginDraw();
					output->DrawImage(RemoteWidth, RemoteHeight, FrameQueue.back());
					output->EndDraw();
					m->unlock();
				}

				bNewFrame = false;

				TotalFrames++;
				double LastFrameTime = PerformanceTimer::GetTicks();
				printf("Overall FPS: %f\n", TotalFrames / (LastFrameTime - FirstFrameTime) * 1000.0);
			}
			//Sleep(SLEEP_AFTER_FRAME_RENDER);
		}
	}
	//wglMakeCurrent(NULL, NULL);
	//ReleaseDC(hWnd, hDC);
	//wglDeleteContext(hRC);

	return msg.wParam;
}

int	ReceiveImages(const char* LocalIpServer, const char* RemoteIpServer, const char* LocalPortServer, const char* RemotePortServer, 
	const char* LocalPortUDPPartner, const char* RemotePortUDPPartner, const char* ListenPort)
{
	const char* connectedIp = nullptr;
	const char* connectedPort = nullptr;
	pNetworking = Networking::getInstance(LocalIpServer, RemoteIpServer, std::string(ListenPort));
	if ((*prefs)["force_remote"] == "1" || pNetworking->ConnectToServer(LocalIpServer, LocalPortServer))
	{
		printf("Connecting to %s:%s failed\n", LocalIpServer, LocalPortServer);
		if (pNetworking->ConnectToServer(RemoteIpServer, RemotePortServer))
		{
			printf("Connecting to %s:%s failed\n", RemoteIpServer, RemotePortServer);
			ErrorExit();
		}
		else
			connectedIp = RemoteIpServer;
	}
	else
		connectedIp = LocalIpServer;

	if (connectedIp == nullptr)
	{
		ERROR_MSG("Connected Ip is null\n");
		return 1;
	}

	printf("%s is our peer now. UDP packets will be sent to this address\n", connectedIp);
	if (!strcmp(connectedIp, partnerIpLocal))
		connectedPort = LocalPortServer;
	if (!strcmp(connectedIp, partnerIpRemote))
		connectedPort = RemotePortServer;

	if (connectedPort == nullptr)
	{
		ERROR_MSG("Connected Port is null <=> could not find partner ip equal to our peer\n");
		return 1;
	}

	if (IsPrivateAddress((char*)connectedIp, true))
	{
		printf("%s:%s is our peer now. UDP packets will be sent to this address\n", connectedIp, LocalPortUDPPartner);
		std::thread *inputSendReceiveThread = new std::thread(SendReceiveInput, connectedIp, LocalPortUDPPartner, listenPort);
	}
	else
	{
		printf("%s:%s is our peer now. UDP packets will be sent to this address\n", connectedIp, RemotePortUDPPartner);
		std::thread *inputSendReceiveThread = new std::thread(SendReceiveInput, connectedIp, RemotePortUDPPartner, listenPort);
	}
	
	while (!bUDPReady);
	printf("UDP Ready\n");
	Sleep(1000);

	std::string command;
	char CommandResponse[6];
	int iResult;
	
	char pcName[201];
	command = "get_pc_name";
	iResult = pNetworking->SendToServer((char*)command.c_str(), command.size());
	iResult = pNetworking->ReceiveFromServer((char*)pcName, 201);
	if (iResult <= 0)
	{
		ERROR_MSG("could not get remote pc's name\n");
		return 1;
	}
	pcName[iResult] = 0;

	char* myName = GetPcName();

	command = "save_my_name";
	iResult = pNetworking->SendToServer((char*)command.c_str(), command.size());
	iResult = pNetworking->SendToServer(myName, strlen(myName));
	iResult = pNetworking->ReceiveFromServer((char*)CommandResponse, 2); // "ok" is expected => hence strlen("ok") is 2
	if (iResult <= 0)
	{
		ERROR_MSG("could not get remote pc's name\n");
		return 1;
	}
	CommandResponse[iResult] = 0;
	if (strcmp(CommandResponse, "ok"))
	{
		ERROR_MSG("remote pc could not save my name(%s)\n", myName);
		return 1;
	}


	command = "get_screen_height";
	iResult = pNetworking->SendToServer((char*)command.c_str(), command.size());
	iResult = pNetworking->ReceiveFromServer((char*)CommandResponse, 5);
	CommandResponse[5] = 0;
	RemoteHeight = atoi(CommandResponse);

	command = "get_screen_width";
	iResult = pNetworking->SendToServer((char*)command.c_str(), command.size());
	iResult = pNetworking->ReceiveFromServer((char*)CommandResponse, 5);
	CommandResponse[5] = 0;
	RemoteWidth = atoi(CommandResponse);

	printf("Screen width of server: %d\n",RemoteWidth);
	printf("Screen height of server: %d\n",RemoteHeight);

	//Block until OpenGL is initialized
	MaxSize = RemoteWidth * RemoteHeight * 4;
	shot = new BYTE[MaxSize];
	partial = new BYTE[MaxSize];
	prevShot = new BYTE[MaxSize];
	difference = new BYTE[MaxSize];
	pngBuffer = new BYTE[MaxSize];

	SetWindowText(hWnd, pcName);

	int i = 0;
	do
	{
		std::string message = std::string("get_frame_data");
		const char* sendbuf = message.c_str();
		int			iResult;

		iResult = pNetworking->SendToServer(sendbuf, strlen(sendbuf));
		if (iResult > 0)
		{
			if (VERBOSE) printf("Bytes Sent: %ld\n", iResult);
			if (VERBOSE) printf("Data sent: %s\n", sendbuf);

			char header[100];
			iResult = pNetworking->ReceiveFromServer(header, 100);

			if (iResult > 0)
			{
				if (strstr(header, "partialframe"))
				{
					DWORD totalDecompressionTime = 0;
					DWORD totalDownloadTime = 0;
					DWORD totalFramePastingTime = 0;
					std::vector<std::future<DecompressReturn>> decompressionThreads;
					std::vector<DecompressReturn> returns;

					int iterations = -1;
					do
					{
						header[iResult] = 0;
						if (VERBOSE) printf("|\n|\n");
						if (VERBOSE) printf("Header received: %s\n", header);

						const char* id = strtok(header, ";");
						const char* left = strtok(NULL, ";");
						const char* top = strtok(NULL, ";");
						const char* num_files = strtok(NULL, ";");
						const char* checksum = strtok(NULL, ";");
						if (iterations == -1)
							iterations = atoi(num_files);
						if (VERBOSE) printf("Iteration #%d\n", iterations);
						if (VERBOSE) printf("Partial frame with left = %d and top = %d\n", left, top);
						if (VERBOSE) printf("Num of frames = %d\n", num_files);
						if (VERBOSE) printf("Checksum = %d\n", checksum);
						
						DWORD befDownload = GetTickCount();
						iResult = pNetworking->ReceiveFromServer((char*)pngBuffer, RemoteWidth*RemoteHeight * 4);
						if (iResult > 0)
						{
							if (VERBOSE) printf("Partial frame data received\n");
						}
						else
						{
							ERROR_MSG("ReceiveFromServer failed: " << iResult);
							ErrorExit();
						}

						DWORD afDownload = GetTickCount();
						if (VERBOSE) printf("Received partialframe\n");
						
						if (VERBOSE) printf("! Verifying checksum !\n");
						int actualChecksum = checksum_mod_1m(pngBuffer, iResult);
						if (actualChecksum == atoi(checksum))
						{
							if (VERBOSE) printf(":) Checksum verified\n");
						}
						else
						{
							ERROR_MSG("Invalid checksum; Received == " << checksum << " != Calculated == " << actualChecksum << "\n");
							MessageBox(nullptr, "Check the log for more info", "Error", MB_OK);
						}
						
						totalDownloadTime += (afDownload - befDownload);
						
						unsigned char* temp_buf = new unsigned char[iResult];
						CopyMemory(temp_buf, pngBuffer, iResult);
						decompressionThreads.push_back(std::async(DecompressThreadFunc, temp_buf, iResult, atoi(left), atoi(top)));
						//DecompressReturn ret = DecompressThreadFunc(temp_buf, iResult, atoi(left), atoi(top));
						//returns.push_back(ret);

						iterations--;
						if (iterations > 0)
						{
							iResult = pNetworking->ReceiveFromServer(header, 30);
							if (iResult < 0)
							{
								ERROR_MSG("ReceiveFromServer failed: "<<iResult);
								ErrorExit();
							}
						}

					} while (iterations > 0);

					DWORD beforeDecomp = GetTickCount();
					for (auto& decompressionThread : decompressionThreads)
					//for (int i = 0; i < returns.size(); i++)
					{
						DecompressReturn ret = decompressionThread.get();
						//DecompressReturn ret = returns[i];
						int Width, Height;
						Width = ret.Width;
						Height = ret.Height;
						unsigned char* bmp = ret.data;

						//DWORD befComp = GetTickCount();
						//DecodeJpg(pngBuffer, iResult, partial, Width, Height);
						//DWORD afterComp = GetTickCount();
						//totalDecompressionTime += (afterComp - befComp);

						if (VERBOSE) printf("Partial frame dimensions: (%d, %d)\n", Width, Height);

						int pitch = Width * 4;
						int iTop = ret.Top;
						int iLeft = ret.Left;
						int ScreenPitch = RemoteWidth * 4;
						int ScreenPitchJpg = Width * 4;

						DWORD befPasting = GetTickCount();
						for (int ii = iTop; ii < iTop + Height; ii++)
						{
							//printf << ii << "\n";
							//printf << sizeRectBytes << std::endl;
							for (int jj = iLeft; jj < iLeft + Width; jj++)
							{
								shot[ii*ScreenPitch + jj * 4] = bmp[(ii - iTop)*ScreenPitchJpg + (jj - iLeft) * 4];
								shot[ii*ScreenPitch + jj * 4 + 1] = bmp[(ii - iTop)*ScreenPitchJpg + (jj - iLeft) * 4 + 1];
								shot[ii*ScreenPitch + jj * 4 + 2] = bmp[(ii - iTop)*ScreenPitchJpg + (jj - iLeft) * 4 + 2];
								shot[ii*ScreenPitch + jj * 4 + 3] = bmp[(ii - iTop)*ScreenPitchJpg + (jj - iLeft) * 4 + 3];
							}
						}
						DWORD afPasting = GetTickCount();
						totalFramePastingTime += (afPasting - befPasting);

						delete bmp;
					}
					DWORD afterDecomp = GetTickCount();

					if (VERBOSE) printf("\n\n--->Total decompression time = %d ms\n\n\n", afterDecomp - beforeDecomp);
					if (VERBOSE) printf("\n\n--->Total download time = %d ms\n\n\n", totalDownloadTime);
					if (VERBOSE) printf("\n\n--->Total frame pasting time = %d ms\n\n\n", totalDownloadTime);

					SavePixelsToFile32bppPBGRA(RemoteWidth, RemoteHeight, RemoteWidth * 4,
						shot, RemoteWidth * RemoteHeight * 4, GUID_ContainerFormatBmp);

					bNewFrame = true;

				}
				else if (strstr(header, "fullframe"))
				{
					iResult = pNetworking->ReceiveFromServer((char*)pngBuffer, RemoteWidth*RemoteHeight * 4);
					if (iResult > 0)
					{

						if (VERBOSE) printf("Bytes received for fullframe: %ld\n", iResult);

						if ((*prefs)["store_frames_on_disk"] == std::string("1"))
						{
							FILE *f = fopen((std::to_string(i) + std::string(".jpg")).c_str(), "wb");
							fwrite(pngBuffer, iResult, 1, f);
							fclose(f);
						}

						WICPixelFormatGUID Format;
						int width, height;

						int Width, Height;
						DecodeJpg(pngBuffer, iResult, shot, Width, Height);

						SavePixelsToFile32bppPBGRA(RemoteWidth, RemoteHeight, RemoteWidth*4, 
							shot, RemoteWidth * RemoteHeight * 4, GUID_ContainerFormatBmp);

						bNewFrame = true;
					}
					else
					{
						ERROR_MSG("ReceiveFromServer failed: "<<iResult);
						ErrorExit();
					}

				}
				else
				{
					//MessageBox(nullptr, "neither partial frame nor fullframe. exiting...", "Error", MB_OK);
					//ExitProcess(0);
				}

			}
			else
			{
				ERROR_MSG("ReceiveFromServer failed: "<<iResult);
				ErrorExit();
			}
		}
		else
		{
			ERROR_MSG("SendToServer failed: "<<iResult);
			ErrorExit();
		}
		i++;

	} while (true);

	return 0;
}
inline HRESULT Direct3D11TakeScreenshot(int &bytesWrittenToFile, std::vector<PartialFrame>& partialFrames, bool& fullFrame)
{
	double b = PerformanceTimer::GetTicks();
	double _bf = PerformanceTimer::GetTicks();

	static int callIndex = -1, cumulated = 0;
	static bool firstCall = true;
	callIndex++;
	fullFrame = false;
	//printf << callIndex<< " Direct3D11TakeScreenshot() start\n";
	

	CComPtrCustom<IDXGIResource> lDesktopResource;
	DXGI_OUTDUPL_FRAME_INFO lFrameInfo;

	double x = PerformanceTimer::GetTicks();
	hr = dxHelper->lDeskDupl->AcquireNextFrame(10000, &lFrameInfo, &lDesktopResource);
	//printf << "TotalMetaBufferSize = " << lFrameInfo.TotalMetadataBufferSize << std::endl;
	//printf << "AcumulatedFrames = " << lFrameInfo.AccumulatedFrames << std::endl;
	//printf << "LastMouseUpdateTime = " << lFrameInfo.LastMouseUpdateTime.HighPart << std::endl;
	//printf << "LastMouseUpdateTime = " << lFrameInfo.LastMouseUpdateTime.LowPart << std::endl;
	//printf << "LastPresentTime = " << lFrameInfo.LastPresentTime.HighPart << std::endl;
	//printf << "LastPresentTime = " << lFrameInfo.LastPresentTime.LowPart << std::endl;
	//printf << "------------------------------" << std::endl;
	if (dxHelper->lAcquiredDesktopImage) { dxHelper->lAcquiredDesktopImage->Release(); } //MessageBeep(0xFFFFFFFF);  }
	double y = PerformanceTimer::GetTicks();
	if (VERBOSE) printf("%d\n",y - x);
	//Beep((y - x) * 100, 100);

 	if (hr== DXGI_ERROR_WAIT_TIMEOUT) { printf ( "Error: DXGI_ERROR_WAIT_TIMEOUT\n"); return hr; }
	else if (hr == DXGI_ERROR_ACCESS_LOST) { printf ( "Error: DXGI_ERROR_ACCESS_LOST\n"); return hr; }
	else if (hr == DXGI_ERROR_INVALID_CALL) { printf ( "Error: DXGI_ERROR_INVALID_CALL - application called AcquireNextFrame without releasing the previous frame\n"); return hr; }
	else if (hr == E_INVALIDARG) { printf ( "Error: E_INVALIDARG\n"); return hr; } 
	if (SUCCEEDED(hr)) {} // printf << "Frame Acquired\n"; }
	if (FAILED(hr)) { printf ( "Failed to acquire next frame\n"); return hr; }

	//printf << "AcquireNextFrame result: " << (int)hr << std::endl;
	
	UINT  required;
	RECT  DirtyRects[MAX_RECTS];
	int   i = 0;
	LONG  area = 0;
	float percentage = 100.0f;
	HRESULT res = dxHelper->lDeskDupl->GetFrameDirtyRects(MAX_RECTS, DirtyRects, &required);

	switch (res)
	{
		case S_OK:
		{
			// Get Dirty Rects
			int Width = dxHelper->lOutputDuplDesc.ModeDesc.Width;
			int Height = dxHelper->lOutputDuplDesc.ModeDesc.Height;

			int valid = 0;
			for (int j = 0; j < required; j++)
			{
				if (DirtyRects[j].top >= 0 && DirtyRects[j].bottom >= 0 && DirtyRects[j].left >= 0 && DirtyRects[j].right >= 0 && DirtyRects[j].bottom <= Height && DirtyRects[j].right <= Width)
				{
					area += (DirtyRects[j].right - DirtyRects[j].left)*(DirtyRects[j].bottom - DirtyRects[j].top);
					valid++;
				}
			}

			percentage = (float)area / (dxHelper->lOutputDuplDesc.ModeDesc.Width*dxHelper->lOutputDuplDesc.ModeDesc.Height) *100.0f;
			if (valid > MAX_FILES_TO_SEND)
				percentage = 100.0f;
		}
	}

	if (VERBOSE) printf("%f%% changed\n", percentage);

	// QI for ID3D11Texture2D
	hr = lDesktopResource->QueryInterface(IID_PPV_ARGS(&dxHelper->lAcquiredDesktopImage));
	if (FAILED(hr)) { printf ( "554 Error\n"); return hr; }
	lDesktopResource.Release();
	if (dxHelper->lAcquiredDesktopImage == nullptr) { printf ( "560 Error: lAcquiredDesktopImage is nullptr\n"); return E_FAIL; }

	// Copy image into GDI drawing texture
	dxHelper->lImmediateContext->CopyResource(dxHelper->lGDIImage, dxHelper->lAcquiredDesktopImage);

	// Draw cursor image into GDI drawing texture
	CComPtrCustom<IDXGISurface1> lIDXGISurface1;
	hr = dxHelper->lGDIImage->QueryInterface(IID_PPV_ARGS(&lIDXGISurface1));

	if (FAILED(hr)) { printf ( "573 Error\n"); return hr; }

	HDC  lHDC;
	lIDXGISurface1->GetDC(FALSE, &lHDC);

	//Draw the (server) mouse
	
	CURSORINFO lCursorInfo = { 0 };
	lCursorInfo.cbSize = sizeof(lCursorInfo);
	auto lBoolres = GetCursorInfo(&lCursorInfo);
	if (lBoolres == TRUE)
	{
		if (lCursorInfo.flags == CURSOR_SHOWING)
		{
			auto lCursorPosition = lCursorInfo.ptScreenPos;
			auto lCursorSize = lCursorInfo.cbSize;
			lIDXGISurface1->GetDC(FALSE, &lHDC);
			// Draw the mouse
			DrawIconEx(lHDC, lCursorPosition.x, lCursorPosition.y, lCursorInfo.hCursor, 0, 0, 0, 0, DI_NORMAL | DI_DEFAULTSIZE);
		}

	}

	lIDXGISurface1->ReleaseDC(nullptr);
	

	// Copy image into CPU access texture
	dxHelper->lImmediateContext->CopyResource(dxHelper->lDestImage, dxHelper->lGDIImage);

	// ------------------------------------------------------------------------------------------------------
	// |																									|
	// |							Copy from CPU access texture to bitmap buffer							| 
	// |																									|
	// |----------------------------------------------------------------------------------------------------|
	IDXGISurface* CopySurface = nullptr;
	hr = dxHelper->lDestImage->QueryInterface(__uuidof(IDXGISurface), (void **)&CopySurface);
	if (FAILED(hr))
	{
		return MessageBox(nullptr, "Failed to QI staging texture into IDXGISurface for pointer", "Error", MB_ICONERROR);
	}
	// Map pixels
	DXGI_MAPPED_RECT MappedSurface;
	hr = CopySurface->Map(&MappedSurface, DXGI_MAP_READ);
	if (FAILED(hr))
	{
		CopySurface->Release();
		CopySurface = nullptr;
		return MessageBox(nullptr, "Failed to map surface for pointer", "Error", MB_ICONERROR);
	}
	// Create CPU access texture
	dxHelper->lDestImage->Release();
	dxHelper->lDestImage = nullptr;

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = dxHelper->lOutputDuplDesc.ModeDesc.Width;
	desc.Height = dxHelper->lOutputDuplDesc.ModeDesc.Height;
	desc.Format = dxHelper->lOutputDuplDesc.ModeDesc.Format;
	desc.ArraySize = 1;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.MipLevels = 1;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	desc.Usage = D3D11_USAGE_STAGING;
	hr = dxHelper->lDevice->CreateTexture2D(&desc, NULL, &dxHelper->lDestImage);

	if (FAILED(hr))
	{
		MessageBox(nullptr, "Failed to create final texture", "Error", MB_ICONERROR);
	}
	if (dxHelper->lDestImage == nullptr)
	{
		MessageBox(nullptr, "Final texture == nullptr", "Error", MB_ICONERROR);
	}
	if (hr == E_OUTOFMEMORY)
	{
		MessageBox(nullptr, "Out Of Memory", "Error", MB_ICONERROR);
	}

	// ------------------------------------------------------------------------------------------------------
	// |																									|
	// |										Pixel processing											|
	// |																									|
	// |----------------------------------------------------------------------------------------------------|
	std::string message;
	switch (hr)
	{
	case D3D11_ERROR_FILE_NOT_FOUND: message = "D3D11_ERROR_FILE_NOT_FOUND"; break;
	case D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS: message = "D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS"; break;
	case D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS: message = "D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS"; break;
	case D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD: message = "D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD"; break;
	case D3DERR_INVALIDCALL: message = "D3DERR_INVALIDCALL"; break;
	case DXGI_ERROR_WAS_STILL_DRAWING: message = "DXGI_ERROR_WAS_STILL_DRAWING"; break;
	case E_FAIL: message = "E_FAIL"; break;
	case E_INVALIDARG: message = "E_INVALIDARG"; break;
	case E_OUTOFMEMORY: message = "E_OUTOFMEMORY"; break;
	case E_NOTIMPL: message = "E_NOTIMPL"; break;
	case S_FALSE: message = "S_FALSE"; break;
	case S_OK: message = "S_OK"; break;

	}
	if (VERBOSE) printf("Map Result: %s", message);
	
	double a = PerformanceTimer::GetTicks();
	//printf << "Time to prepare a frame " << a - b << " ms" << std::endl;

	double bf = PerformanceTimer::GetTicks();

	DWORD dwBefore, dwAfter;

	dwBefore = PerformanceTimer::GetTicks();
	CHAR lMyDocPath[MAX_PATH];
	SHGetFolderPath(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, lMyDocPath);

	std::string lFilePath = "tmp.jpg";

	if (percentage < MAX_THRESHOLD_SEND_BITMAP_CHANGED_REGIONS) // change < MAX_THRESHOLD%
	{
		std::vector<std::future<CompressReturn>> compressionThreads;

		//CreateDirectory(std::to_string(callIndex).c_str(), NULL);
		for (int j = 0; j < required; j++)
		{
			int Width = dxHelper->lOutputDuplDesc.ModeDesc.Width;
			int Height = dxHelper->lOutputDuplDesc.ModeDesc.Height;

			if (DirtyRects[j].top >= 0 && DirtyRects[j].bottom >= 0 && DirtyRects[j].left >= 0 && DirtyRects[j].right >= 0 && DirtyRects[j].bottom <= Height && DirtyRects[j].right <= Width)
			{
				std::string lFilePath = std::to_string(callIndex) + "\\" + std::to_string(DirtyRects[j].left) + "_"+ std::to_string(DirtyRects[j].top) + ".jpg";

				int sizeRectBytes = (DirtyRects[j].right - DirtyRects[j].left)*(DirtyRects[j].bottom - DirtyRects[j].top) * 4;
				BYTE* copyRect = new BYTE[sizeRectBytes];

				int pitch = (DirtyRects[j].right - DirtyRects[j].left) * 4;

				for (int ii = DirtyRects[j].top; ii < DirtyRects[j].bottom; ii++)
				{
					for (int jj = DirtyRects[j].left; jj < DirtyRects[j].right; jj++)
					{
						int indexCopyRect = (ii - DirtyRects[j].top)*pitch + (jj - DirtyRects[j].left) * 4;
						copyRect[indexCopyRect] = ((PBYTE)MappedSurface.pBits)[ii*MappedSurface.Pitch + jj * 4];
						copyRect[indexCopyRect + 1] = ((PBYTE)MappedSurface.pBits)[ii*MappedSurface.Pitch + jj * 4 + 1];
						copyRect[indexCopyRect + 2] = ((PBYTE)MappedSurface.pBits)[ii*MappedSurface.Pitch + jj * 4 + 2];
						copyRect[indexCopyRect + 3] = ((PBYTE)MappedSurface.pBits)[ii*MappedSurface.Pitch + jj * 4 + 3];
					}
				}
				//SavePixelsToJpg(DirtyRects[j].right - DirtyRects[j].left,
				//	DirtyRects[j].bottom - DirtyRects[j].top, copyRect, lFilePath.c_str());

				//partialFrames.push_back(PartialFrame(std::string(lFilePath), DirtyRects[j].left, DirtyRects[j].top));
				compressionThreads.push_back(std::async(SavePixelsToJpgMemory, 
														DirtyRects[j].right - DirtyRects[j].left, 
														DirtyRects[j].bottom - DirtyRects[j].top, 
														copyRect, 
														DirtyRects[j].left, 
														DirtyRects[j].top
														)
												);
				
			}
		}

		for (auto& compressionThread : compressionThreads)
		{
			CompressReturn ret = compressionThread.get();
			partialFrames.push_back(PartialFrame(ret.data, ret.Size, ret.Left, ret.Top, checksum_mod_1m(ret.data, ret.Size)));
		}
	}
	else
	{ // The MAX_THRESHOLD is smaller than the screen change => send the whole image
		SavePixelsToJpg(dxHelper->lOutputDuplDesc.ModeDesc.Width,
			dxHelper->lOutputDuplDesc.ModeDesc.Height, (LPBYTE)MappedSurface.pBits, lFilePath.c_str());
		fullFrame = true;
	}

	dwAfter = PerformanceTimer::GetTicks();

	if (VERBOSE) printf("Jpg: %d ms Change = %f%%\n", dwAfter - dwBefore, percentage);

	hr = dxHelper->lDeskDupl->ReleaseFrame();
	dxHelper->lImmediateContext->Flush();
	if (FAILED(hr)) { printf ( "Error: could not release frame\n"); return hr; }
	
	if (percentage >= MAX_THRESHOLD_SEND_BITMAP_CHANGED_REGIONS)
	{
		FILE* f = fopen(lFilePath.c_str(), "rb");
		fseek(f, 0, SEEK_END);
		int size = ftell(f);
		fseek(f, 0, SEEK_SET);
		fread(pngBuffer, size, 1, f);
		//printf << "Direct3D11TakeScreenshot() end with " << hr << "\n";
		bytesWrittenToFile = size;
		fclose(f);
	}

	firstCall = false;
	
	double af = PerformanceTimer::GetTicks();
	double _af = PerformanceTimer::GetTicks();
	//printf << "Time to post-prepare a frame " << af - bf << " ms" << std::endl;

	//printf << "Theoretial total " << (af - bf) + (a - b) << " ms" << std::endl;
	//printf << "Actual total " << (_af - _bf) << " ms" << std::endl;
	return hr;
}

wchar_t *convertCharArrayToLPCWSTR(const char* charArray)
{
	wchar_t* wString = new wchar_t[strlen(charArray)+1];
	MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, strlen(charArray) + 1);
	return wString;
}

// get the dxgi format equivilent of a wic format
DXGI_FORMAT GetDXGIFormatFromWICFormat(WICPixelFormatGUID& wicFormatGUID)
{
	if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFloat) 
	{
		printf("GUID_WICPixelFormat128bppRGBAFloat\n");
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAHalf) 
	{
		printf("GUID_WICPixelFormat64bppRGBAHalf\n");
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBA) 
	{
		printf("GUID_WICPixelFormat64bppRGBA\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA) 
	{
		printf("GUID_WICPixelFormat64bppRGBA\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGRA) 
	{
		//printf("GUID_WICPixelFormat32bppBGRA\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR)
	{
		printf("GUID_WICPixelFormat32bppBGR\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102XR) 
	{
		printf("GUID_WICPixelFormat32bppRGBA1010102XR\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102) 
	{
		printf("GUID_WICPixelFormat32bppRGBA1010102\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGRA5551) 
	{
		printf("GUID_WICPixelFormat16bppBGRA5551\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR565) 
	{
		printf("GUID_WICPixelFormat16bppBGR565\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFloat) 
	{
		printf("GUID_WICPixelFormat32bppGrayFloat\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayHalf) 
	{
		printf("GUID_WICPixelFormat16bppGrayHalf\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGray)
	{
		printf("GUID_WICPixelFormat16bppGray\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat8bppGray) 
	{
		printf("GUID_WICPixelFormat8bppGray\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else if (wicFormatGUID == GUID_WICPixelFormat8bppAlpha) 
	{
		printf("GUID_WICPixelFormat8bppAlpha\n");
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}
	else
	{
		printf("DXGI_FORMAT_UNKNOWN\n");
		return DXGI_FORMAT_UNKNOWN;
	}
}

HRESULT DecodeImageFromBuffer(BYTE* inBuffer, int inSize, BYTE* outBuffer, int outSize, int& width, int& height, WICPixelFormatGUID& Format)
{
	HRESULT hr = S_OK;
	// WIC interface pointers.
	IWICStream *pIWICStream = NULL;
	IWICBitmapDecoder *pIDecoder = NULL;
	IWICBitmapFrameDecode *pIDecoderFrame = NULL;
	IWICImagingFactory *pIWICFactory;
	BOOL coInit = CoInitialize(nullptr);

	// Create WIC factory
	hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory));

	// Create a WIC stream to map onto the memory.
	if (SUCCEEDED(hr))
		hr = pIWICFactory->CreateStream(&pIWICStream);
	else
		printf("CreateStream failed with code %#08X\n",hr);

	// Initialize the stream with the memory pointer and size.
	if (SUCCEEDED(hr))
		hr = pIWICStream->InitializeFromMemory(reinterpret_cast<BYTE*>(inBuffer), inSize);

	// Create a decoder for the stream.
	if (SUCCEEDED(hr)) 
	{
		hr = pIWICFactory->CreateDecoderFromStream(
			pIWICStream,                   // The stream to use to create the decoder
			NULL,                          // Do not prefer a particular vendor
			WICDecodeMetadataCacheOnLoad,  // Cache metadata when needed
			&pIDecoder);                   // Pointer to the decoder
	}
	else
		printf("InitializeFromMemory failed\n");

	// Retrieve the first bitmap frame.
	if (SUCCEEDED(hr))
	{
		hr = pIDecoder->GetFrame(0, &pIDecoderFrame);
		if (SUCCEEDED(hr));
		else
		{
			printf("GetFrame failed\n");
			goto cleanup;
		}
		WICPixelFormatGUID PixelFormat;
		pIDecoderFrame->GetPixelFormat(&PixelFormat);
		UINT textureWidth, textureHeight;
		hr = pIDecoderFrame->GetSize(&textureWidth, &textureHeight);
		Format = PixelFormat;
		DXGI_FORMAT dxgiFormat = GetDXGIFormatFromWICFormat(PixelFormat);
		
		//printf("Width %d Height %d Format %#08X\n", textureWidth, textureHeight, PixelFormat);
		width = textureWidth;
		height = textureHeight;
		pIDecoderFrame->CopyPixels(0, textureWidth * 4, outSize, outBuffer);
	}
	else
		printf("CreateDecoderFromStream failed\n");

cleanup:
	if (pIWICStream) RELEASE(pIWICStream);
	if (pIDecoderFrame) RELEASE(pIDecoderFrame);
	if (pIDecoder) RELEASE(pIDecoder);
	if (pIWICFactory) RELEASE(pIWICFactory);
	if (coInit) CoUninitialize();
	return hr;
}

HRESULT SavePixelsToFile32bppPBGRA(UINT width, UINT height, UINT stride, LPBYTE pixels, int bufsize, const GUID& format)
{
	DWORD b = GetTickCount();

	HRESULT hr = S_OK;
	IWICImagingFactory *factory = nullptr;
	IWICBitmapEncoder *encoder = nullptr;
	IWICBitmapFrameEncode *frame = nullptr;
	IWICBitmap *bitmap = nullptr;
	IWICStream *stream = nullptr;
	GUID pf = GUID_WICPixelFormat32bppPBGRA;
	BOOL coInit = CoInitialize(nullptr);

	IWICBitmap* bmpframe;
	HRCHECK(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
	HRCHECK(factory->CreateBitmapFromMemory(width, height, pf, stride, bufsize, pixels, &bmpframe));
	FrameQueue.push(bmpframe);

cleanup:
	RELEASE(factory);
	if (coInit) CoUninitialize();
	return hr;
}

int	DecodeJpg(BYTE* _compressedImage, int _jpegSize, BYTE* outBuffer, int& Width, int& Height)
{
	int jpegSubsamp, width, height;
	tjhandle _jpegDecompressor = tjInitDecompress();
	tjDecompressHeader2(_jpegDecompressor, _compressedImage, _jpegSize, &width, &height, &jpegSubsamp);
	tjDecompress2(_jpegDecompressor, _compressedImage, _jpegSize, outBuffer, width, 0/*pitch*/, height, TJPF_BGRX, TJFLAG_FASTDCT);
	tjDestroy(_jpegDecompressor);
	Width = width;
	Height = height;
	return 0;
}

int	SavePixelsToJpg(UINT width, UINT height, LPBYTE pixels, const char* filePath)
{
	tjhandle _jpegCompressor = tjInitCompress();
	long unsigned int _jpegSize = 0;
	unsigned char* _compressedImage = nullptr;
	const int JPEG_QUALITY = ImageQuality;

	tjCompress2(_jpegCompressor, (unsigned char*)pixels, width, 0, height, TJPF_BGRX,
		&_compressedImage, &_jpegSize, TJSAMP_444, JPEG_QUALITY,
		TJFLAG_FASTDCT);

	FILE* f = fopen(filePath, "wb");
	fwrite(_compressedImage, _jpegSize, 1, f);
	fclose(f);

	tjDestroy(_jpegCompressor);
	//to free the memory allocated by TurboJPEG (either by tjAlloc(), 
	//or by the Compress/Decompress) after you are done working on it:
	tjFree(_compressedImage);

	return 0;
}

CompressReturn SavePixelsToJpgMemory(UINT width, UINT height, LPBYTE pixels, int left, int top)
{
	CompressReturn ret;
	tjhandle _jpegCompressor = tjInitCompress();
	long unsigned int _jpegSize = 0;
	unsigned char* _compressedImage = nullptr;
	const int JPEG_QUALITY = ImageQuality;

	int res = tjCompress2(_jpegCompressor, (unsigned char*)pixels, width, 0, height, TJPF_BGRX,
		&_compressedImage, &_jpegSize, TJSAMP_444, JPEG_QUALITY,
		TJFLAG_FASTDCT);
	if (res == -1)
	{
		MessageBox(nullptr, (std::string("Error compressing the jpg: ") + std::string(tjGetErrorStr())).c_str(), "Error", MB_OK);
		while (true);
	}
	delete pixels;
	ret.data = _compressedImage;
	ret.Size = _jpegSize;
	ret.Left = left;
	ret.Top = top;
	return ret;

}

void CopySegment(void* dest, void* source, int start, int end)
{
	CopyMemory((void*)((int)(dest) + start), (void*)((int)source + start), (end - start));
}

BOOL SendReceiveInput(const char* IpPartner, const char* PortPartner, const char* ListenPort)
{
	printf("SendReceiveInput('%s','%s','%s')\n", IpPartner, PortPartner, ListenPort);
	const int recv_expire = 3000;

	if (Broadcaster)
	{
		printf("receiving events on port %s only from %s:%s\n", ListenPort, IpPartner, PortPartner);
		// Create a connectionless socket
		SOCKET sUDPSocket = Networking::getBoundUDPSocket(atoi(ListenPort), recv_expire);

		// Check to see if we have a valid socket
		if (sUDPSocket == INVALID_SOCKET) 
		{
			ErrorExit();
		}

		printf("Sending 10 UDP punch packets to %s:%s\n", IpPartner, PortPartner);
		for (int i = 0; i < 10; i++)
		{
			int sent = Networking::sendUDPPacket(sUDPSocket, IpPartner, PortPartner, (char*)"test", 4);
			if (sent <= 0)
			{
				ERROR_MSG("sent <= 0");
				ErrorExit();
			}
			printf("packet sent: %s", "test\n");
			Sleep(UDP_TEST_PACKET_DELAY);
		}

		bUDPReady = true;

		// Receive a datagram from another device
		char cBuffer[1024];
		int nBytesRecv = 0;
		int nBufSize = 1024;
		int nReceiveAddrSize = 0;

		while (TRUE)
		{
			// Get the datagram
			int iResult = recvfrom(sUDPSocket, cBuffer, nBufSize, 0,
				nullptr, 0);
			if (iResult == -1)
			{
				//if (WSAGetLastError() != 10014) {
				if (WSAGetLastError() == WSAETIMEDOUT)
				{
					ERROR_MSG("recvfrom timeout");
					//MessageBox(nullptr, "recvfrom timeout", "error", MB_OK);
				}
				else
				{
					ERROR_MSG("winsock error code " << WSAGetLastError());
				}
				continue;
			}
			cBuffer[iResult] = 0;
			//MessageBox(nullptr, cBuffer, "remote msg", MB_OK);
			if (std::string(cBuffer) == std::string("handle_input_event"))
			{
				//printf << cBuffer << std::endl;
				UINT uMsg;
				WPARAM wParam;
				LPARAM lParam;
				char data[20];
				iResult = recvfrom(sUDPSocket, data, 20, 0,
					nullptr, 0);
				if (iResult <= 0)
				{
					printf("recv failed for uMsg: %d", iResult);
					continue;
				}

				//printf("uMsg = %s\n", data);
				uMsg = atoi(data);

				iResult = recvfrom(sUDPSocket, data, 20, 0,
					nullptr, 0);
				if (iResult <= 0)
				{
					printf("recv failed for uMsg: %d", iResult);
					continue;
				}

				//printf("wParam = %s\n", data);
				wParam = atoi(data);

				iResult = recvfrom(sUDPSocket, data, 20, 0,
					nullptr, 0); 
				if (iResult <= 0)
				{
					printf("recv failed for uMsg: %d", iResult);
					continue;
				}

				//printf("lParam = %s\n", data);
				lParam = atoi(data);

				INPUT RemoteInputEvent;
				switch (uMsg)
				{
				case WM_MOUSEMOVE:
					//printf("\nWM_MOUSEMOVE\n");
					//printf("\n\n\n-----------%d , %d---------\n\n\n", LOWORD(lParam), HIWORD(lParam));
					RemoteInputEvent.type = INPUT_MOUSE;
					//system("cls");
					//printf << LOWORD(lParam) << "   " << HIWORD(lParam);
					
					SetCursorPos((int)(dxHelper->lOutputDuplDesc.ModeDesc.Width/1000.0f*LOWORD(lParam)),
						(int)(dxHelper->lOutputDuplDesc.ModeDesc.Height / 1000.0f*HIWORD(lParam))
					);

					break;
				case WM_KEYDOWN:
				case WM_KEYUP:
					//printf("\WM_KEY|DOWN/UP\n");
					RemoteInputEvent.type = INPUT_KEYBOARD;
					RemoteInputEvent.ki.wVk = wParam;
					RemoteInputEvent.ki.time = 0;
					RemoteInputEvent.ki.wScan = MapVirtualKeyEx(wParam, 0, (HKL)0xf0010413);
					if (uMsg == WM_KEYUP)
						RemoteInputEvent.ki.dwFlags = KEYEVENTF_KEYUP;
					else
						RemoteInputEvent.ki.dwFlags = 0;
					SendInput(1, &RemoteInputEvent, sizeof(INPUT));
					break;
				case WM_LBUTTONDOWN:
					//printf("\WM_LBUTTONDOWN\n");
					RemoteInputEvent.type = INPUT_MOUSE;
					RemoteInputEvent.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTDOWN;
					RemoteInputEvent.mi.dx = 0;
					RemoteInputEvent.mi.dy = 0;
					RemoteInputEvent.mi.mouseData = 0;
					RemoteInputEvent.mi.time = GetTickCount();
					RemoteInputEvent.mi.dwExtraInfo = 0;
					SendInput(1, &RemoteInputEvent, sizeof(INPUT));
					break;
				case WM_LBUTTONUP:
					//printf("\WM_LBUTTONUP\n");
					RemoteInputEvent.type = INPUT_MOUSE;
					RemoteInputEvent.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTUP;
					RemoteInputEvent.mi.dx = 0;
					RemoteInputEvent.mi.dy = 0;
					RemoteInputEvent.mi.mouseData = 0;
					RemoteInputEvent.mi.time = GetTickCount();
					RemoteInputEvent.mi.dwExtraInfo = 0;
					SendInput(1, &RemoteInputEvent, sizeof(INPUT));
					break;
				case WM_RBUTTONDOWN:
					//printf("\WM_RBUTTONUP\n");
					RemoteInputEvent.type = INPUT_MOUSE;
					RemoteInputEvent.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_RIGHTDOWN;
					RemoteInputEvent.mi.dx = 0;
					RemoteInputEvent.mi.dy = 0;
					RemoteInputEvent.mi.mouseData = 0;
					RemoteInputEvent.mi.time = GetTickCount();
					RemoteInputEvent.mi.dwExtraInfo = 0;
					SendInput(1, &RemoteInputEvent, sizeof(INPUT));
					break;
				case WM_RBUTTONUP:
					//printf("\WM_RBUTTONUP\n");
					RemoteInputEvent.type = INPUT_MOUSE;
					RemoteInputEvent.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_RIGHTUP;
					RemoteInputEvent.mi.dx = 0;
					RemoteInputEvent.mi.dy = 0;
					RemoteInputEvent.mi.mouseData = 0;
					RemoteInputEvent.mi.time = GetTickCount();
					RemoteInputEvent.mi.dwExtraInfo = 0;
					SendInput(1, &RemoteInputEvent, sizeof(INPUT));
					break;
				case WM_MOUSEWHEEL:
					//printf("\WM_MOUSEWHEEL\n");
					break;

				}
			}

			
		}
		

		// Close the socket
		closesocket(sUDPSocket);
		WSACleanup();
	}
	else
	{
		printf("sending events from port %s to %s:%s\n", ListenPort, IpPartner, PortPartner);
		// Create a connectionless socket
		SOCKET sUDPSocket = Networking::getBoundUDPSocket(atoi(ListenPort), recv_expire);

		printf("Server initialized for sending input\n");

		// Check to see if we have a valid socket
		if (sUDPSocket == INVALID_SOCKET) {
			ErrorExit();
		}

		// Send a datagram to the target device
		char cBuffer[1024] = "Test Buffer";
		int nBytesSent = 0;
		int nBufSize = strlen(cBuffer);

		
		printf("Sending 10 UDP punch packets to %s:%s\n", IpPartner, PortPartner);
		for (int i = 0; i < 10; i++)
		{
			int sent = Networking::sendUDPPacket(sUDPSocket, IpPartner, PortPartner, (char*)"test", 4);
			if (sent <= 0)
			{
				ERROR_MSG("sent <= 0");
				ErrorExit();
			}
			printf("packet sent: %s", "test\n");
			Sleep(UDP_TEST_PACKET_DELAY);
		}

		sockaddr_in sTargetDevice;

		bUDPReady = true;

		while (TRUE)
		{
			while (Events.size() > 0)
			{
				//printf("Sending input message\n");
				Event InputMsg = Events.front();
				Events.pop();

				//int sent = sendto(sUDPSocket, "handle_input_event", strlen("handle_input_event"), 0,
				//	(SOCKADDR *)&sTargetDevice,
				//	sizeof(SOCKADDR_IN)); 
				int sent = Networking::sendUDPPacket(sUDPSocket, IpPartner, PortPartner, (char*)"handle_input_event", strlen("handle_input_event"));
				
				if (sent <= 0)
				{
					printf("sending 'handle_input_event' failed: %d", sent);
					printf("Error code: %d\n", WSAGetLastError());
					break;
				}
				//--------------------SENDING UMSG---------------------
				char data[20];
				sprintf(data, "%d", InputMsg.uMsg);
				//sent = sendto(sUDPSocket, data, 20, 0,
				//	(SOCKADDR *)&sTargetDevice,
				//	sizeof(SOCKADDR_IN));
				sent = Networking::sendUDPPacket(sUDPSocket, IpPartner, PortPartner, data, 20);
				if (sent <= 0)
				{
					printf("sending %s failed: %d", data, sent);
					break;
				}

				//--------------------SENDING WPARAM---------------------
				sprintf(data, "%d", InputMsg.wParam);
				//sent = sendto(sUDPSocket, data, 20, 0,
				//	(SOCKADDR *)&sTargetDevice,
				//	sizeof(SOCKADDR_IN));
				sent = Networking::sendUDPPacket(sUDPSocket, IpPartner, PortPartner, data, 20);
				if (sent <= 0)
				{
					printf("sending %s failed: %d", data, sent);
					break;
				}

				//--------------------SENDING LPARAM---------------------
				sprintf(data, "%d", InputMsg.lParam);
				//sent = sendto(sUDPSocket, data, 20, 0,
				//	(SOCKADDR *)&sTargetDevice,
				//	sizeof(SOCKADDR_IN));
				sent = Networking::sendUDPPacket(sUDPSocket, IpPartner, PortPartner, data, 20);
				if (sent <= 0)
				{
					printf("sending %s failed: %d", data, sent);
					break;
				}

				//printf << "Input message sent\n";
			}
		}

		// Close the socket
		closesocket(sUDPSocket);
	}


	return FALSE;
}

DecompressReturn DecompressThreadFunc(unsigned char* _compressedImage, int _jpegSize, int left, int top)
{
	DecompressReturn ret;
	int jpegSubsamp;

	tjhandle _jpegDecompressor = tjInitDecompress();

	int res = tjDecompressHeader2(_jpegDecompressor, _compressedImage, _jpegSize, &ret.Width, &ret.Height, &jpegSubsamp);
	if (res == -1)
	{
		std::string message = std::string("Error for tjDecompressHeader2: ") + std::string(tjGetErrorStr());
		MessageBox(nullptr, message.c_str(), "Error", MB_ICONERROR);
		while (true);
	}
	ret.data = new unsigned char[ret.Width*ret.Height * 4];

	res = tjDecompress2(_jpegDecompressor, _compressedImage, _jpegSize, ret.data, ret.Width, 0/*pitch*/, ret.Height, TJPF_BGRX, TJFLAG_FASTDCT);
	if (res == -1)
	{
		std::string message = std::string("Error for tjDecompress2: ") + std::string(tjGetErrorStr());
		MessageBox(nullptr, message.c_str(), "Error", MB_ICONERROR);
		while (true);
	}

	tjDestroy(_jpegDecompressor);
	delete _compressedImage;

	ret.Left = left;
	ret.Top = top;

	if (VERBOSE) printf("Successfuly exited from %d_%d_%d_%d thread\n", ret.Left,ret.Top,ret.Width,ret.Height);
	return ret;
}

int checksum_mod_1m(unsigned char* data, int size)
{
	int sum = 0;
	int incr = 10;
	for (int i = 0; i < size; i += incr)
	{
		sum += data[i];
		sum %= 1000000;
	}
	return sum;
}