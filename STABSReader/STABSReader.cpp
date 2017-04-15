// STABSReader.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <locale.h>

#include "STABSymtab.h"

int _tmain(int argc, _TCHAR* argv[])
{
	setlocale(LC_ALL, "chs");
	_tcout.imbue( std::locale(std::locale(), "chs", LC_CTYPE) ); 

	CStabSymtab s;
	s.Load(_T("E:\\MinGW\\msys\\1.0\\home\\hackpascal\\glib-build\\gobject\\glib-genmarshal.exe"));

	return 0;
}

