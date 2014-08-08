#include "stdafx.h"

extern "C" int mainCRTStartup();

#include "windows.h"

int EntryPoint()
{
	volatile char tlsBuf[4096];

	//VCµÄCRTÈë¿Ú
	return mainCRTStartup();
}

