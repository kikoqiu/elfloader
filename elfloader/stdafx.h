// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef __CYGWIN__
#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here
#include <Windows.h>

#else
#include <Windows.h>
#define _T(x) x
#endif

#include <stdio.h>
#include <iostream>