#include "stdafx.h"

extern "C" int mainCRTStartup();

#include "windows.h"

int EntryPoint()
{
	volatile char tlsBuf[4096];

	//VC��CRT���
	return mainCRTStartup();
}

