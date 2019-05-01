#pragma once

enum EventType
{
	WIN32_UI,
	CHAT
};

class Event
{
public:
	WPARAM wParam;
	LPARAM lParam;
	UINT uMsg;
	EventType et;
	char* message;

	Event(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		this->wParam = wParam;
		this->lParam = lParam;
		this->uMsg = uMsg;
		this->et = WIN32_UI;
		this->message = nullptr;
	}
	Event(const char* message)
	{
		this->message = (char*)message;
		this->et = CHAT;
	}
	Event(char* message)
	{
		this->message = message;
		this->et = CHAT;
	}

	EventType getType()
	{
		return et;
	}
};