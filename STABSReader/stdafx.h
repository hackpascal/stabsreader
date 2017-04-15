// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: 在此处引用程序需要的其他头文件
#include <Windows.h>
#include <strsafe.h>

#include <string>
#include <iostream>

#ifdef UNICODE
#define TS	L"ls"
#define _tostringstream std::wostringstream
#define _tstring std::wstring
#define _tcout std::wcout
#else
#define TS	"s"
#define _tostringstream std::ostringstream
#define _tstring std::string
#define _tcout std::cout
#endif