// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: �ڴ˴����ó�����Ҫ������ͷ�ļ�
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