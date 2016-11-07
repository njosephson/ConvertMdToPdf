// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>


#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
using namespace std;

#ifdef _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>  // for WideCharToMultiByte
#endif

#include "hoedown/src/html.h"
#include "hoedown/src/version.h"

#include "wkhtmltox\pdf.h"
#pragma comment(lib, "wkhtmltox.lib")
