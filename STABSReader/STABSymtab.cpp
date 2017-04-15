#include "stdafx.h"

#include <stack>

#include "STABSymtab.h"

// 指定下一个字符是 c，否则格式错误
#define NEXT_CHAR(c) \
	if (*tmp != (c)) throw SYMTAB_EXCEPTION_BAD_FORMAT;

// 指定下一个字符是 c，并递增，否则格式错误
#define NEXT_CHAR_INC(c) \
	if (*tmp != (c)) throw SYMTAB_EXCEPTION_BAD_FORMAT; \
	else ++tmp;

// 如果下一个字符是 c，则递增
#define OPT_CHAR_INC(c) \
	if (*tmp == (c)) ++tmp;

// 跳过一个字符
#define SKIP_CHAR \
	++tmp;

// 跳至指定字符，否则格式错误
#define JUMP_CHAR(c) \
	p = strchr(tmp, (c)); \
	if (!p) throw SYMTAB_EXCEPTION_BAD_FORMAT; \
	else tmp = p;

// 跳至指定字符，并递增，否则格式错误
#define JUMP_CHAR_INC(c) \
	p = strchr(tmp, (c)); \
	if (!p) throw SYMTAB_EXCEPTION_BAD_FORMAT; \
	else tmp = p + 1;

CStabSymtab::CStabSymtab()
{
	m_hFile = NULL;
	m_hFileMap = NULL;
	m_pMem = NULL;

	m_pStabs = NULL;
	m_pStabStr = NULL;
	m_dwStabsCount = 0;
	m_dwStabStrSize = 0;

	m_dwCurrentFile = NONE;
	m_nUnnamedIndex = 0;
}

BOOL CStabSymtab::Load(LPCTSTR lpFileName)
{
	BOOL bResult;
	m_hFile = CreateFile(lpFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (m_hFile == NULL || m_hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	bResult = Load(m_hFile);
	CloseHandle(m_hFile);
	m_hFile = NULL;

	return bResult;
}

BOOL CStabSymtab::Load(HANDLE hFile)
{
	BOOL bResult;
	m_hFile = hFile;
	m_hFileMap = CreateFileMapping(m_hFile, NULL, PAGE_READONLY, 0, 0, NULL);

	if (m_hFileMap == NULL || m_hFileMap == INVALID_HANDLE_VALUE)
		return FALSE;

	m_pMem = MapViewOfFile(m_hFileMap, FILE_MAP_READ, 0, 0, 0);

	if (!m_pMem)
	{
		CloseHandle(m_hFileMap);
		m_hFileMap = NULL;
		return FALSE;
	}

	bResult = LoadFromMemory(m_pMem);

	UnmapViewOfFile(m_pMem);
	CloseHandle(m_hFileMap);
	m_hFileMap = NULL;
	m_pMem = NULL;

	return bResult;
}

BOOL CStabSymtab::LoadFromMemory(LPVOID lpMem)
{
	CHAR pszSectionName[9];
	BOOL bResult;

	m_pMem = lpMem;
	m_pStabs = NULL;
	m_pStabStr = NULL;

	PIMAGE_DOS_HEADER pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(m_pMem);
	PIMAGE_NT_HEADERS pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<PCHAR>(pDosHeader) + pDosHeader->e_lfanew);
	PIMAGE_SECTION_HEADER pSectionHeader = reinterpret_cast<PIMAGE_SECTION_HEADER>(reinterpret_cast<PCHAR>(pNtHeaders) + sizeof (DWORD) + sizeof (IMAGE_FILE_HEADER) + pNtHeaders->FileHeader.SizeOfOptionalHeader);

	m_b64Bit = pNtHeaders->OptionalHeader.Magic == 0x20B;

	ZeroMemory(pszSectionName, sizeof (pszSectionName));
	for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++)
	{
		StringCchCopyNA(pszSectionName, 9, reinterpret_cast<LPSTR>(pSectionHeader[i].Name), 8);
		if (!strcmp(pszSectionName, ".stab"))
		{
			m_pStabs = reinterpret_cast<internal_nlist *>(reinterpret_cast<PCHAR>(m_pMem) + pSectionHeader[i].PointerToRawData);
			m_dwStabsCount = pSectionHeader[i].SizeOfRawData / sizeof (internal_nlist);
		}

		if (!strcmp(pszSectionName, ".stabstr"))
		{
			m_pStabStr = reinterpret_cast<LPSTR>(reinterpret_cast<PCHAR>(m_pMem) + pSectionHeader[i].PointerToRawData);
			m_dwStabStrSize = pSectionHeader[i].SizeOfRawData;
		}
	}

	if (!m_pStabs || !m_pStabStr)
	{
		m_pStabs = NULL;
		m_pStabStr = NULL;
		return FALSE;
	}

	bResult = ParseDebugInformation();

	m_pStabs = NULL;
	m_pStabStr = NULL;
	return bResult;
}

BOOL CStabSymtab::ParseDebugInformation()
{
	DWORD it = 0;
	LPSTR lpszName;
	std::string sSourcePath; 
	BOOL bSourceIsFile = FALSE;
	internal_nlist stabempty;

	m_dwCurrentFile = 0xffffffffUL;
	ZeroMemory(&stabempty, sizeof (internal_nlist));

	PGLOBAL_TYPE pType;
	PFUNCTION pCurrentFunc = NULL;
	PGLOBAL_TYPE pFuncProtoType = NULL;
	PVARIABLE pVar = NULL;
	std::stack<PCODE_BLOCK> BlockLayers;

	//FILE *f;
	//fopen_s(&f, "E:\\cc1_stabs.txt", "w");

	while (it < m_dwStabsCount)
	{
//		fprintf(stderr, "%d\n\r", it);
		if (!memcmp(&m_pStabs[it], &stabempty, sizeof (internal_nlist)))
			break;

		//fprintf_s(f, "%02lx\t%u\t%u\t%08lx\t%s\n", (int) m_pStabs[it].n_type, (int) m_pStabs[it].n_other, (int) m_pStabs[it].n_desc, m_pStabs[it].n_value, m_pStabStr + m_pStabs[it].n_strx);
		//*
		try
		{
			switch(m_pStabs[it].n_type)
			{
			case N_SO:	// 获取当前源代码文件的路径
				VALIDATE_STRING_ELSE(m_pStabs[it].n_strx)
				{
					// 表明上一个源代码文件已结束
					sSourcePath = "";
					m_dwCurrentFile = NONE;
					break;
				}

				lpszName = m_pStabStr + m_pStabs[it].n_strx;

				if (DirectoryExists(lpszName))
				{
					// 是个目录
					sSourcePath = lpszName;
					for (size_t i = 0; i < sSourcePath.size(); i++)
						if (sSourcePath[i] == '/') sSourcePath[i] = '\\';
					bSourceIsFile = FALSE;
				}
				else
				{
					if (FileExists(lpszName))
					{
						// 是个文件
						sSourcePath = lpszName;
						for (size_t i = 0; i < sSourcePath.size(); i++)
							if (sSourcePath[i] == '/') sSourcePath[i] = '\\';
						bSourceIsFile = TRUE;
						m_dwCurrentFile = AddSourceFile(sSourcePath.c_str());
						printf("\nFile: %s\n", sSourcePath.c_str());
					}
					else
					{
						if (!sSourcePath.empty())
						{
							// 组合目录和当前字符串
							std::string sTemp(sSourcePath);
							if (sTemp.back() != '\\') sTemp.push_back('\\');
							sTemp.append(lpszName);

							// 判断是否是文件
							if (FileExists(sTemp.c_str()))
							{
								sSourcePath = sTemp;
								for (size_t i = 0; i < sSourcePath.size(); i++)
									if (sSourcePath[i] == '/') sSourcePath[i] = '\\';
								bSourceIsFile = TRUE;
								m_dwCurrentFile = AddSourceFile(sSourcePath.c_str());
								printf("\nFile: %s\n", sSourcePath.c_str());
							}
							else
							{
								// 什么都不是
								sSourcePath.clear();
								bSourceIsFile = FALSE;
							}
						}
					}
				}

				break;

			case N_LSYM: // 全局类型 函数的局部变量
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				if (it == 3008) //2452
				{
					int a=0;
				}
				if (!pCurrentFunc)
				{
					pType = ResolveType(lpszName, 0, NULL, TRUE, TRUE);
					//PrintType(pType);
				}
				else
				{
					if (BlockLayers.size())
					{
						pVar = ResolveVariable(lpszName, TRUE, FALSE);
						pVar->wLineNum = m_pStabs[it].n_desc;
						AddCodeBlockVariable(BlockLayers.top(), pVar);
					}
				}
				break;
			case N_RSYM: // 寄存器变量/参数
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				if (pCurrentFunc && pFuncProtoType)
				{
					pVar = ResolveVariable(lpszName, TRUE, FALSE);

					PVARIABLE pParam = GetFunctionParameter(pFuncProtoType, pVar->lpName);

					if (pParam)
					{
						pParam->dwRegisterNum = pVar->dwRegisterNum;
						pParam->Type = ParameterRegister;
						DestroyVariable(pVar);
					}
					else
					{
						pVar->wLineNum = m_pStabs[it].n_desc;
						AddCodeBlockVariable(BlockLayers.top(), pVar);
					}
				}
				break;
			case N_STSYM: // 全局变量 (已初始化，静态)
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				pVar = ResolveVariable(lpszName);
				pVar->llAddress = m_pStabs[it].n_value;
				break;
			case N_LCSYM: // 全局变量 (未始化)
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				pVar = ResolveVariable(lpszName);
				pVar->llAddress = m_pStabs[it].n_value;
				break;
			case N_GSYM: // 全局变量 (已初始化，需在符号表查找地址)
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				pVar = ResolveVariable(lpszName);
				break;
			case N_FUN: // 函数 既可能是开始，也可能是结束，需要从 n_strx 判断
				VALIDATE_STRING_ELSE(m_pStabs[it].n_strx)
				{
					if (pCurrentFunc) pCurrentFunc->dwCodeSize = m_pStabs[it].n_value + 1;
					PrintFunction(pCurrentFunc);
					pCurrentFunc = NULL;
					pFuncProtoType = NULL;
					break;
				}
				lpszName = m_pStabStr + m_pStabs[it].n_strx;


				pCurrentFunc = ResolveFunction(lpszName);
				pCurrentFunc->dwAddress = m_pStabs[it].n_value;
				pFuncProtoType = pCurrentFunc->pType;

				while (BlockLayers.size()) BlockLayers.pop();
				BlockLayers.push(pCurrentFunc->pFirstBlock);
				break;
			case N_SLINE: // 函数的行号信息
				if (pCurrentFunc)
				{
					AddFunctionLineNumber(pCurrentFunc, m_pStabs[it].n_desc, m_pStabs[it].n_value);
				}
				break;
			case N_PSYM: // 函数的参数
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				pVar = ResolveVariable(lpszName, TRUE, FALSE);
				if (pCurrentFunc && pFuncProtoType)
				{
					AddFunctionParameter(pFuncProtoType, pVar);
					pVar->llAddress = m_pStabs[it].n_value;
					pVar->wLineNum = m_pStabs[it].n_desc;
				}
				else
				{
					DestroyVariable(pVar);
				}
				break;
			case N_LBRAC: // 语句块起始位置
				if (m_pStabs[it].n_value)
					if (BlockLayers.size())
					{
						PCODE_BLOCK pNewBlock = CreateCodeBlock(BlockLayers.top());
						pNewBlock->dwStartOffset = m_pStabs[it].n_value;
						BlockLayers.push(pNewBlock);
					}
				break;
			case N_RBRAC: // 语句块结束位置
				if (BlockLayers.size())
				{
					PCODE_BLOCK pCodeBlock = BlockLayers.top();
					BlockLayers.pop();
					pCodeBlock->dwSize = m_pStabs[it].n_value - pCodeBlock->dwStartOffset + 1;
				}
				break;
			case N_BNSYM: // 函数开始的另一个标志
				break;
			case N_ENSYM: // 函数结束的另一个标志
				pCurrentFunc = NULL;
				pFuncProtoType = NULL;
				break;

			case N_UNDF:// 未定义
			case N_SOL: // #include 的文件
			case N_OPT: // 标志，GCC 中表示此文件由 gcc 编译
			case N_BINCL: // #include 开始
			case N_EINCL: // #include 结束
			case N_EXCL: // 已删除的 #include 占位符 (即 #include 这行本身)
				break;
			default:
				throw SYMTAB_EXCEPTION_FIXME;
				break;
			}
		}
		catch (int a)
		{
			switch (a)
			{
			case SYMTAB_EXCEPTION_UNKNOWN:
				_tprintf(_T("未知异常\n"));
				break;
			case SYMTAB_EXCEPTION_FIXME:
				_tprintf(_T("FIXME 异常\n"));
				break;
			case SYMTAB_EXCEPTION_BAD_ALLOCATION:
				_tprintf(_T("内存分配失败\n"));
				break;
			case SYMTAB_EXCEPTION_BAD_FORMAT:
				_tprintf(_T("格式错误\n"));
				break;
			case SYMTAB_EXCEPTION_UNKNOWN_TYPE:
				_tprintf(_T("未知类型\n"));
				break;
			case SYMTAB_EXCEPTION_UNKNOWN_RANGE:
				_tprintf(_T("未知子界范围\n"));
				break;
			}
		}
		//*/
		it++;
	}

	//fclose(f);

	return TRUE;
}

// 查找 STABS 类型名称的结束位置
LPSTR CStabSymtab::GetEndOfName(LPSTR lpStr)
{
	// 真没有名称时
	if (*lpStr == '(')
		return lpStr;
	if (*lpStr == '*')
		return lpStr;
	if (*lpStr == ',')
		return lpStr;

	LPSTR p = strchr(lpStr, ':');
	if (!p) return lpStr;
	if (p == lpStr) return lpStr;
	
	// 两个冒号 (::) 是 C++ 中的特有符号
	while (p && p[1] == ':')
	{
		p = strchr(p + 2, ':');
		if (!p) return lpStr;
	};
	return p;
}

// 生成临时类型名
LPTSTR CStabSymtab::DumpName()
{
	TCHAR pszName[255];
	LPTSTR lpszFinalName;
		
	// 使用 __unnamed__<num>__ 形式的名字
	*pszName = 0;
	_stprintf_s(pszName, 255, _T("__unnamed__%x__"), m_nUnnamedIndex++);

	lpszFinalName = new TCHAR[_tcslen(pszName) + 1];
	StringCchCopy(lpszFinalName, _tcslen(pszName) + 1, pszName);

	return lpszFinalName;
}

// 获取类型/变量/函数/参数名，并将其转换为合适的编码
LPTSTR CStabSymtab::DumpName(LPSTR &lpName, BOOL bNoAnonymous)
{
	LPSTR p, lpszTemp;
	LPTSTR lpszFinalName;
	int cchSize;

	p = GetEndOfName(lpName);

	if (p == lpName)
	{
		// 使用临时类型名
		if (!bNoAnonymous)
			return DumpName();
		else
			return _tcsdup(_T(""));
	}

	lpszTemp = new CHAR[static_cast<size_t>(p - lpName) + 1];
	StringCchCopyNA(lpszTemp, static_cast<size_t>(p - lpName) + 1, lpName, static_cast<size_t>(p - lpName));
#ifdef UNICODE
	cchSize = MultiByteToWideChar(CP_ACP, 0, lpszTemp, -1, NULL, NULL);
	lpszFinalName = new TCHAR[cchSize];
	MultiByteToWideChar(CP_ACP, 0, lpszTemp, -1, lpszFinalName, cchSize);
	delete [] lpszTemp;
#else
	lpszFinalName = lpszTemp;
#endif
	lpName = p;
	return lpszFinalName;
}

// 获取类型的唯一标识符
DWORD CStabSymtab::GetUniqueId(LPSTR &lpStr)
{
	LPSTR tmp = lpStr;
	DWORD wFileId, wTypeId;

	NEXT_CHAR_INC('(');
	wFileId = strtoul(tmp, &tmp, 10);
	NEXT_CHAR_INC(',');
	wTypeId = strtoul(tmp, &tmp, 10);
	NEXT_CHAR_INC(')');
	lpStr = tmp;
	// 等同于 MAKEDWORD(wFileId, wTypeId)
	// 但这个 wFileId 不同于 m_cCurrentFile
	return (wFileId << 16) | (wTypeId & 0xffff);
}

// 根据子界类型确定基本数据类型
ENUM_BASIC_TYPE CStabSymtab::GetBasicType(LPSTR &lpSubrange)
{

#define LBOUND(t) \
	{ Result = (t); goto LBound; }
#define UBOUND(t) \
	{ Result = (t); goto UBound; }

	ENUM_BASIC_TYPE Result;
	LPSTR p;

	// 这样写是为了避免进行字符串到数字的转换
	// 同时也是为了偷懒
	if (!strncmp(lpSubrange, "-128;", 5) || !strncmp(lpSubrange, "0200;", 5))
		LBOUND(Char);
	if (!strncmp(lpSubrange, "-32768;", 7) || !strncmp(lpSubrange, "0100000;", 8))
		LBOUND(Short);
	if (!strncmp(lpSubrange, "-2147483648;", 12) || !strncmp(lpSubrange, "020000000000;", 13))
		LBOUND(Int);
	if (!strncmp(lpSubrange, "-9223372036854775808;", 21) || !strncmp(lpSubrange, "01000000000000000000000;", 24))
		LBOUND(Longlong);
	// 浮点类型
	if (!strncmp(lpSubrange, "4;", 2))
		LBOUND(Single);
	if (!strncmp(lpSubrange, "8;", 2))
		LBOUND(Double);
	if (!strncmp(lpSubrange, "12;", 2))
		LBOUND(LongDouble);
	if (!strncmp(lpSubrange, "16;", 2))
		LBOUND(Extended);
	if (!strncmp(lpSubrange, "0;", 2))
	{
		// 获取下界
		if (!(p = strchr(lpSubrange, ';')))
			throw SYMTAB_EXCEPTION_UNKNOWN_RANGE;

		lpSubrange = p + 1;

		// 这个特殊，是 7 位的 char 类型。将其视作 8 位有符号的 char
		if (!strncmp(lpSubrange, "127;", 4))
			UBOUND(Char);
		// 无符号类型
		if (!strncmp(lpSubrange, "255;", 4) || !strncmp(lpSubrange, "0377;", 5))
			UBOUND(Byte);
		if (!strncmp(lpSubrange, "65535;", 6) || !strncmp(lpSubrange, "0177777;", 8))
			UBOUND(Word);
		if (!strncmp(lpSubrange, "4294967295;", 11) || !strncmp(lpSubrange, "037777777777;", 13))
			UBOUND(Dword);
		if (!strncmp(lpSubrange, "18446744073709551615;", 21) || !strncmp(lpSubrange, "01777777777777777777777;", 24))
			UBOUND(Qword);

		// FIXME: 没有考虑到或无效类型
		throw SYMTAB_EXCEPTION_UNKNOWN_RANGE;
	}

	// FIXME: 没有考虑到或无效类型
	throw SYMTAB_EXCEPTION_UNKNOWN_RANGE;

#undef LBOUND
#undef UBOUND
	
LBound:
	if (!(p = strchr(lpSubrange, ';')))
		return None;

	lpSubrange = p + 1;
UBound:
	if (!(p = strchr(lpSubrange, ';')))
		lpSubrange = lpSubrange + strlen(lpSubrange);
	else
		lpSubrange = p + 1;

	return Result;
}

// 确定浮点/复数类型
ENUM_BASIC_TYPE CStabSymtab::GetComplexType(LPSTR &lpSubrange)
{
	ENUM_BASIC_TYPE Result;
	DWORD num;
	LPSTR tmp = lpSubrange;

	num = strtoul(tmp, &tmp, 10);
	NEXT_CHAR_INC(';');
	NEXT_CHAR_INC('0');
	NEXT_CHAR_INC(';');

	switch (num)
	{
	case 8:
		Result = Complex;
		break;
	case 16:
		Result = Complex16;
		break;
	case 24:
		Result = Complex24;
		break;
	case 32:
		Result = Complex32;
		break;
	default:
		throw SYMTAB_EXCEPTION_FIXME;
	}

	lpSubrange = tmp;
	return Result;
}

// 解析类型
PGLOBAL_TYPE CStabSymtab::ResolveType(LPSTR &lpStabStr, DWORD dwLastId, LPTSTR lpszTypeName, BOOL bResolveVoid, BOOL bAddToList)
{
	LPSTR tmp = lpStabStr;
	ENUM_BASIC_TYPE BaseType, CurrentType;
	DWORD dwUniqueId;		// 当前类型的 Id
	DWORD dwAliasId;
	DWORD dwLastArrayId;	// 上一个数组维度的 Id
	DWORD dwNum;
	BOOL bForwardDecl = FALSE;
	BOOL bIsClass = FALSE;
	LARGE_INTEGER llDim;
	CHAR cFpType;
	DWORD dwLBound, dwUBound;
	
	LPTSTR lpszMainName = NULL;
	PGLOBAL_TYPE pTempType = NULL;
	PGLOBAL_TYPE pFirstType = NULL;
	PSYMTAB_ARRAY_DIMS pArrayDims = NULL;
	PSYMTAB_VARIABLE_LIST pMembers = NULL;
	PVARIABLE pNewVar = NULL;

	if (*lpStabStr == 0 || !lpStabStr)
		return NULL;

	if (!lpszTypeName)
		lpszMainName = DumpName(tmp);
	
	OPT_CHAR_INC(':');

	try
	{
		switch (*tmp)
		{
		case 't':	// [第一级] 带名称的类型
		case 'T':
			SKIP_CHAR;

			if (*tmp == 't')
			{
				// C++ 代码， struct class union 自带 typedef 类型
				SKIP_CHAR;
			}

			dwUniqueId = GetUniqueId(tmp);

			// 表明是一个已存在的类型
			if (*tmp == 0)
			{
				DWORD dwFileId = m_dwCurrentFile;

				if (!pFirstType)
				{
					while (!pFirstType && static_cast<int>(dwFileId) >= 0)
						pFirstType = GetGlobalType(dwUniqueId, dwFileId--);
				}

				if (pFirstType)
				{
					if (pFirstType->dwFileId == dwFileId)
					{
						if (AddGlobalTypeAlias(pFirstType, dwUniqueId, lpszMainName))
							lpszMainName = NULL;

						if (!pFirstType->dwMasterId)
						{
							pFirstType->dwMasterId = dwUniqueId;
							pFirstType->bNew = TRUE;
						}
					}
					else
						pFirstType->dwCurrentId = pFirstType->dwMasterId;
					break;
				}

				throw SYMTAB_EXCEPTION_UNKNOWN_TYPE;
			}

			NEXT_CHAR_INC('=');

			pArrayDims = new SYMTAB_ARRAY_DIMS;
			dwLastArrayId = 0;
			
			// FIXME: 其实数组不应该这样处理。。。
			// STABS 中定义数组的方式不同于 C/C++，而是类似于 Pascal 的链式结构：
			// val: array[l1..h1] of array[l2..h2] of .. of array[ln..hn] of Type;
			// val:(x,y1)=a;l1;h1;(x,y2)=a;l2;h2;...(x,yn)=a;ln;hn;(Type)

			// 解析类型，若为数组，则保存数组维度，并继续解析
			pTempType = ResolveType(tmp, dwUniqueId, lpszMainName, bResolveVoid, TRUE);
			while (pTempType->BasicType == Array && pTempType->bNew)
			{
				// 是数组类型，此时只解析出了一维，将其保存，继续解析
				pArrayDims->push_back(pTempType->pDims->operator[](0));
				// 保存数组 Id
				dwLastArrayId = pTempType->dwCurrentId;
				// 当解析出的类型为数组时，此类型不会被添加到类型列表中
				// 因此必须销毁它
				DestroyGlobalType(pTempType);
				// 继续解析
				pTempType = ResolveType(tmp, dwUniqueId, lpszMainName, bResolveVoid, TRUE);
			}

			// 数组类型，此时的 pFirstType 是数组的类型
			if (pArrayDims->size())
			{
				// 新建数组类型
				pFirstType = CreateGlobalType(m_dwCurrentFile);
				pFirstType->BasicType = Array;
				pFirstType->pTypeTo = pTempType;
				pFirstType->dwTypeAliasTo = pTempType->dwMasterId;
				pFirstType->pDims = pArrayDims;
				AddGlobalType(pFirstType);
				pArrayDims = NULL;

				// FIXME: 一个 hack，保留此一维数组 Id，并使之成为此类型的别名
				if (pFirstType->pDims->size() == 1)
					AddGlobalTypeAlias(pFirstType, dwLastArrayId);
			}
			else
			{
				pFirstType = pTempType;
			}

			pTempType = NULL;

			if (AddGlobalTypeAlias(pFirstType, dwUniqueId, lpszMainName))
				lpszMainName = NULL;

			if (pFirstType->dwMasterId == 0) pFirstType->dwMasterId = dwUniqueId;

			break;
		case '@':	// [第1.5级] 暂时不考虑此数据，跳过
			SKIP_CHAR;
			NEXT_CHAR_INC('s');
			while (isdigit(*tmp)) tmp++;
			NEXT_CHAR_INC(';');

			pFirstType = ResolveType(tmp, dwLastId, _T(""), bResolveVoid, bAddToList);
			break;
		case 'r':	// [第二级] Subrange, 子界类型 (说白了就是基本类型)
			SKIP_CHAR;
			dwAliasId = GetUniqueId(tmp);
			NEXT_CHAR_INC(';');

			BaseType = GetBasicType(tmp);

			pFirstType = CreateGlobalType(m_dwCurrentFile);
			pFirstType->BasicType = BaseType;
			AddGlobalTypeAlias(pFirstType, dwAliasId);

			if (bAddToList)
				AddGlobalType(pFirstType);

			break;	
		case 'R':	// [第二级] 内建浮点类型
			SKIP_CHAR;

			if (!isdigit(*tmp)) throw SYMTAB_EXCEPTION_BAD_FORMAT;
			cFpType = *tmp;
			SKIP_CHAR;
			NEXT_CHAR_INC(';');

			BaseType = GetComplexType(tmp);

			// 有些不是复数类型
			switch (cFpType)
			{
			case '1': BaseType = Single; break;
			case '2': BaseType = Double; break;
			case '3':
			case '4':
			case '5':
				break;
			case '6':
				 BaseType = LongDouble; break;
				 break;
			default:
				// FIXME: STABS 手册上都没有的类型
				throw SYMTAB_EXCEPTION_FIXME;
			}

			pFirstType = CreateGlobalType(m_dwCurrentFile);
			pFirstType->BasicType = BaseType;

			if (bAddToList)
					AddGlobalType(pFirstType);
			break;
		case '-':	// [第二级] 负数标识，表示内建类型
			SKIP_CHAR;

			dwNum = strtoul(tmp, &tmp, 10);

			switch (dwNum)
			{
			case 1: case 4: case 15: case 16: case 29:
				BaseType = Int;
				break;
			case 2: case 6: case 27:
				BaseType = Char;
				break;
			case 3: case 28:
				BaseType = Short;
				break;
			case 5:
				BaseType = Byte;
				break;
			case 7: case 22: case 30:
				BaseType = Word;
				break;
			case 8: case 9: case 10: case 23: case 24:
				BaseType = Dword;
				break;
			case 11:
				BaseType = Void;
				break;
			case 12: case 17:
				BaseType = Single;
				break;
			case 13: case 18:
				BaseType = Double;
				break;
			case 14:
				BaseType = LongDouble;
				break;
			case 19:
				BaseType = Pointer;
				break;
			case 20: case 21:
				BaseType = Byte;
				break;
			case 25:
				BaseType = Complex;
				break;
			case 26:
				BaseType = Complex16;
				break;
			case 31: case 34:
				BaseType = Longlong;
				break;
			case 32: case 33:
				BaseType = Qword;
				break;
			default:
				throw SYMTAB_EXCEPTION_FIXME;
			}

			pFirstType = CreateGlobalType(m_dwCurrentFile);
			pFirstType->BasicType = BaseType;

			if (bAddToList)
					AddGlobalType(pFirstType);

			break;
		case 'c':	// [第二级] 带初始值的静态全局变量
			// FIXME: 现在没法处理，忽略
			pFirstType = NULL;
			break;
		case 'B':	// [第二极] volatile
			SKIP_CHAR;
		case '(':	// [第二级] Void 类型、别名、不带名称的类型
			dwAliasId = GetUniqueId(tmp);
		
			// Void 类型
			if (bResolveVoid && (dwAliasId == dwLastId))
			{
				pFirstType = CreateGlobalType(m_dwCurrentFile);
				pFirstType->BasicType = Void;
				AddGlobalTypeAlias(pFirstType, dwAliasId);

				if (bAddToList)
					AddGlobalType(pFirstType);

				break;
			}
		
			// 类型别名
			if (*tmp != '=')
			{
				DWORD dwFileId = m_dwCurrentFile;
				pFirstType = GetGlobalType(dwAliasId, dwFileId);

				if (!pFirstType)
				{
					if (lpszTypeName && *lpszTypeName)
						while (!pFirstType && static_cast<int>(dwFileId) >= 0)
							pFirstType = GetGlobalType(lpszTypeName, --dwFileId);

					if (!pFirstType)
					{
						dwFileId = m_dwCurrentFile;
						while (!pFirstType && static_cast<int>(dwFileId) >= 0)
							pFirstType = GetGlobalType(dwAliasId, --dwFileId);
					}
				}

				if (pFirstType)
				{
					// 已有类型，因此设置 bNew 为 FALSE 表示不是新类型
					pFirstType->bNew = FALSE;

					if (pFirstType->dwFileId == dwFileId)
						pFirstType->dwCurrentId = dwAliasId;
					else
						pFirstType->dwCurrentId = pFirstType->dwMasterId;
					break;
				}

				// FIXME: 找不到类型，默认为 PVOID

				if (!(pFirstType = GetGlobalType(_T("PVOID"))))
					pFirstType = GetGlobalType(_T("int"));

				break;
			}

			// 无名称的新类型
			NEXT_CHAR_INC('=');

			pFirstType = ResolveType(tmp, dwAliasId, _T(""), bResolveVoid, bAddToList);
			AddGlobalTypeAlias(pFirstType, dwAliasId);

			break;
		case 'k':	// [第三级] 常量类型
		case '*':	// [第三级] 指针类型
		case 'x':	// [第三级] 前向声明
			if (*tmp == 'k') CurrentType = Const;
			if (*tmp == '*') CurrentType = Pointer;
			if (*tmp == 'x') { CurrentType = Pointer; bForwardDecl = TRUE; }
			SKIP_CHAR;

			pFirstType = CreateGlobalType(m_dwCurrentFile);

			if (!bForwardDecl)
			{
				pTempType = ResolveType(tmp, dwLastId, _T(""), bResolveVoid, TRUE);
				if (pTempType->bNew)
				{
					if (!pTempType->dwMasterId) pTempType->dwMasterId = pTempType->dwCurrentId;
				}

				pFirstType->BasicType = CurrentType;
				pFirstType->pTypeTo = pTempType;
				pFirstType->dwTypeAliasTo = pTempType->dwCurrentId;

				pTempType = NULL;
			}
			else
			{
				switch (*tmp)
				{
				case 's':
					pFirstType->BasicType = Structure;
					break;
				case 'u':
					pFirstType->BasicType = Union;
					break;
				case '2':
					pFirstType->BasicType = Enum;
					break;
				default:
					// FIXME: 还有其他类型么
					throw SYMTAB_EXCEPTION_FIXME;
				}
				SKIP_CHAR;
			}			

			if (bForwardDecl)
			{
				if (!lpszMainName)
				{
					switch (pFirstType->BasicType)
					{
					case Structure: case Union: case Enum:
						lpszMainName = DumpName(tmp);
						NEXT_CHAR_INC(':');
					}
				}

				pFirstType->dwMasterId = dwLastId;
			}

			if (AddGlobalTypeAlias(pFirstType, dwLastId, lpszMainName))
				lpszMainName = NULL;

			if (bAddToList) 
				AddGlobalType(pFirstType);

			break;
		case 'f':	// [第三级] 函数声明，只有一个唯一 Id，没有参数和返回类型
			SKIP_CHAR;
			// 设置函数标志位
			dwAliasId = GetUniqueId(tmp) | 0x80000000UL;

			if (!(pFirstType = GetGlobalType(dwAliasId, m_dwCurrentFile)))
			{
				pFirstType = CreateGlobalType(m_dwCurrentFile);
				pFirstType->BasicType = Function;

				AddGlobalTypeAlias(pFirstType, dwAliasId);

				if (bAddToList)
					AddGlobalType(pFirstType);
			}

			if (*tmp == '=')
			{
				// FIXME: 这后面多余的内容是干嘛的？
				SKIP_CHAR;
				pTempType = ResolveType(tmp, dwLastId, _T(""), FALSE, FALSE);
				DestroyGlobalType(pTempType);
			}

			break;
		case 'a':	// [第三极] 数组类型
			SKIP_CHAR;
			// 只解析一个维度，然后由上一级调用者来组合多维数组，
			// 否则将会解析成链式的数组类型，即
			// 一维数组 -> 一维数组 -> ... -> 一维数组 -> 具体类型
			// 具体说明参见第一级的说明

			// 'a' 后面跟着 'r'，说明数组的范围类型是 Subrange 类型
			if (*tmp == 'r')
			{
				SKIP_CHAR;
				pFirstType = ResolveType(tmp, dwLastId, _T(""), bResolveVoid, TRUE);
			}

			NEXT_CHAR_INC(';');
			llDim.LowPart = strtoul(tmp, &tmp, 10);
			NEXT_CHAR_INC(';');
			llDim.HighPart = strtoul(tmp, &tmp, 10);
			NEXT_CHAR_INC(';');
			
			pFirstType = CreateGlobalType(m_dwCurrentFile);
			pFirstType->BasicType = Array;
			pFirstType->pDims = new SYMTAB_ARRAY_DIMS;
			pFirstType->pDims->push_back(llDim);

			AddGlobalTypeAlias(pFirstType, dwLastId);
			break;
		case 's':	// [第三级] 结构体/联合体类型
		case 'u':
			if (*tmp == 's') CurrentType = Structure;
			if (*tmp == 'u') CurrentType = Union;
			SKIP_CHAR;

			// 建立声明，以便于使用自身指针时能够找到类型
			if (!(pFirstType = GetGlobalType(dwLastId, m_dwCurrentFile)))
			{
				LPTSTR lpszTemp;
				pFirstType = CreateGlobalType(m_dwCurrentFile);

				if (lpszTypeName == NULL || *lpszTypeName == 0)
					lpszTemp = NULL; // 结构/联合体是匿名的
				else
					lpszTemp = _tcsdup(lpszTypeName);

				if (AddGlobalTypeAlias(pFirstType, dwLastId, lpszTemp))
					lpszTemp = NULL;

				AddGlobalType(pFirstType);
				DestroyCString(lpszTemp);
			}

			pFirstType->BasicType = CurrentType;

			// 跳过最开始的数字，这个数字是结构体/联合体的字节数
			while (isdigit(*tmp)) SKIP_CHAR;

			pMembers = new SYMTAB_VARIABLE_LIST;

			while (*tmp != ';')
			{
				cFpType = '2';
				// 类成员可见性修饰符
				if (*tmp == '/')
				{
					SKIP_CHAR;
					cFpType = *tmp;
					SKIP_CHAR;
					bIsClass = TRUE;
				}

				pNewVar = ResolveVariable(tmp, TRUE, FALSE);

				NEXT_CHAR_INC(',');
				dwLBound = strtoul(tmp, &tmp, 10);
				NEXT_CHAR_INC(',');
				dwUBound = strtoul(tmp, &tmp, 10);
				NEXT_CHAR_INC(';');

				pNewVar->llAddress = 0;
				pNewVar->dwBitStart = dwLBound;
				pNewVar->dwBitLength = dwUBound;
				pNewVar->Type = Member;

				switch (cFpType)
				{
				case '0': pNewVar->Visibility = Private; break;
				case '1': pNewVar->Visibility = Protected; break;
				case '2': pNewVar->Visibility = Public; break;
				case '9': pNewVar->Visibility = Public; break;
				default:
					throw SYMTAB_EXCEPTION_FIXME;
				}

				pMembers->push_back(pNewVar);
				pNewVar = NULL;
			}
			tmp++;

			DestroyMembers(pFirstType->pMembers);
			pFirstType->pMembers = pMembers;

			if (bIsClass) pFirstType->BasicType = Class;

			pMembers = NULL;
			break;
		case 'e':	// [第三级] 枚举类型
			SKIP_CHAR;

			pMembers = new SYMTAB_VARIABLE_LIST;

			while (*tmp != ';')
			{
				lpszMainName = DumpName(tmp);

				NEXT_CHAR_INC(':');
				dwLBound = strtoul(tmp, &tmp, 10);
				NEXT_CHAR_INC(',');

				pNewVar = CreateVariable();
				pNewVar->dwTypeAliasId = dwLBound;
				pNewVar->Type = Member;
				pNewVar->lpName = lpszMainName;

				pMembers->push_back(pNewVar);
				pNewVar = NULL;
				lpszMainName = NULL;
			}
			NEXT_CHAR_INC(';');
			
			pFirstType = CreateGlobalType(m_dwCurrentFile);
			pFirstType->BasicType = Enum;

			DestroyMembers(pFirstType->pMembers);
			pFirstType->pMembers = pMembers;

			AddGlobalTypeAlias(pFirstType, dwLastId);
			AddGlobalType(pFirstType);

			pMembers = NULL;
			break;
		default:
			// FIXME: 没有考虑到的类型
			throw SYMTAB_EXCEPTION_FIXME;
		}
		lpStabStr = tmp;
	}
	catch (int a)
	{
		DestroyGlobalType(pFirstType);
		DestroyCString(lpszMainName);
		DestroyGlobalType(pTempType);
		DestroyArrayDims(pArrayDims);
		DestroyMembers(pMembers);
		DestroyVariable(pNewVar);
		throw a;
	}
	catch (...)
	{
		DestroyGlobalType(pFirstType);
		DestroyCString(lpszMainName);
		DestroyGlobalType(pTempType);
		DestroyArrayDims(pArrayDims);
		DestroyMembers(pMembers);
		DestroyVariable(pNewVar);
		throw SYMTAB_EXCEPTION_UNKNOWN;
	}
	
	DestroyCString(lpszMainName);
	DestroyGlobalType(pTempType);
	DestroyArrayDims(pArrayDims);
	DestroyMembers(pMembers);
	DestroyVariable(pNewVar);
	return pFirstType;
}

// 解析变量
PVARIABLE CStabSymtab::ResolveVariable(LPSTR &lpStabStr, BOOL bResolveName, BOOL bAddToList)
{
	LPSTR tmp = lpStabStr;
	DWORD dwTypeId;		// 变量的类型 Id
	DWORD dwLastArrayId;	// 上一个数组维度的 Id
	PVARIABLE pNewVar = NULL;
	LPTSTR lpszMainName = NULL;
	PGLOBAL_TYPE pTempType = NULL, pNewType = NULL;
	PSYMTAB_ARRAY_DIMS pArrayDims = NULL;

	if (bResolveName)
		lpszMainName = DumpName(tmp, TRUE);

	if (*tmp == ':') tmp++;
	
	pNewVar = CreateVariable();

	try
	{
		switch (*tmp)
		{
		case 'G':	// 全局变量，需在符号表中查找绝对地址
		case 'S':	// 全局静态变量
		case 'V':	// 函数静态变量
		case 'p':	// 函数参数
		case 'r':	// 寄存器变量/参数
		case 'P':	// 寄存器参数
			if (*tmp == 'G') pNewVar->Type = Global;
			if (*tmp == 'S') pNewVar->Type = GlobalStatic;
			if (*tmp == 'V') pNewVar->Type = LocalStatic;
			if (*tmp == 'p') pNewVar->Type = Parameter;
			if (*tmp == 'r') pNewVar->Type = LocalRegister;
			if (*tmp == 'P') pNewVar->Type = ParameterRegister;
			SKIP_CHAR;
		case '(':	// 类型
			pArrayDims = new SYMTAB_ARRAY_DIMS;				
			dwLastArrayId = 0;
			dwTypeId = 0;

			// bResolveVoid 参数必须设置为 FALSE, 以防止将新类型解析为 void
			pTempType = ResolveType(tmp, 0, _T(""), FALSE, TRUE);
			while (pTempType->BasicType == Array && pTempType->bNew)
			{
				// 是数组类型，此时只解析出了一维，将其保存，继续解析
				pArrayDims->push_back(pTempType->pDims->operator[](0));
				// 保存此数组 Id
				if (!dwTypeId) dwTypeId = pTempType->dwCurrentId;
				dwLastArrayId = pTempType->dwCurrentId;
				// 当解析出的类型为数组时，此类型不会被添加到类型列表中
				// 因此必须销毁它
				DestroyGlobalType(pTempType);
				pTempType = ResolveType(tmp, 0, _T(""), FALSE, TRUE);
			}

			// 数组类型，此时的 pFirstType 是数组指向的类型
			if (pArrayDims->size())
			{
				pNewType = CreateGlobalType(m_dwCurrentFile);
				pNewType->BasicType = Array;
				pNewType->pTypeTo = pTempType;
				pNewType->dwTypeAliasTo = pTempType->dwMasterId;
				pNewType->pDims = pArrayDims;
				pArrayDims = NULL;

				AddGlobalType(pNewType);
				AddGlobalTypeAlias(pNewType, dwTypeId);
				pNewType->dwMasterId = dwTypeId;

				// FIXME: 一个 hack，使一维数组等同于使用此数组的类型
				if (pNewType->pDims->size() == 1)
					AddGlobalTypeAlias(pNewType, dwLastArrayId);
			}
			else
			{
				pNewType = pTempType;
			}

			pTempType = NULL;

			if (pNewType->dwMasterId == 0) pNewType->dwMasterId = pNewType->dwCurrentId;

			pNewVar->dwFileId = m_dwCurrentFile;
			pNewVar->lpName = lpszMainName;
			pNewVar->pType = pNewType;
			pNewVar->dwTypeAliasId = pNewType->dwCurrentId;
			pNewType = NULL;
			lpszMainName = NULL;
		
			if (bAddToList)
				AddVariable(pNewVar);

			lpStabStr = tmp;
			break;
		default:
			// FIXME: 未考虑到的类型
			throw SYMTAB_EXCEPTION_FIXME;
		}
	}
	catch (int a)
	{
		DestroyVariable(pNewVar);
		DestroyCString(lpszMainName);
		DestroyGlobalType(pTempType);
		DestroyGlobalType(pNewType);
		DestroyArrayDims(pArrayDims);
		throw a;
	}
	catch (...)
	{
		DestroyVariable(pNewVar);
		DestroyCString(lpszMainName);
		DestroyGlobalType(pTempType);
		DestroyGlobalType(pNewType);
		DestroyArrayDims(pArrayDims);
		throw SYMTAB_EXCEPTION_UNKNOWN;
	}
	
	DestroyCString(lpszMainName);
	DestroyGlobalType(pTempType);
	DestroyGlobalType(pNewType);
	DestroyArrayDims(pArrayDims);
	return pNewVar;
}

// 解析函数
PFUNCTION CStabSymtab::ResolveFunction(LPSTR &lpStabStr)
{
	LPSTR tmp = lpStabStr;
	DWORD dwTypeId;		// 变量的类型 Id
	PFUNCTION pNewFunc = NULL;
	PGLOBAL_TYPE pNewType = NULL;
	LPTSTR lpszMainName = NULL;

	lpszMainName = DumpName(tmp);

	if (*tmp == ':') tmp++;

	try
	{
		switch (*tmp)
		{
		case 'f':
		case 'F':
			SKIP_CHAR;
			dwTypeId = GetUniqueId(tmp) | 0x80000000UL;

			// 建立函数原型
			if (!(pNewType = GetGlobalType(dwTypeId, m_dwCurrentFile)))
			{
				pNewType = CreateGlobalType(m_dwCurrentFile);
				pNewType->BasicType = Function;
			}

			// 设置函数名  // FIXME: 函数原型类型不应该有名字
//			if (AddGlobalTypeAlias(pNewType, dwTypeId, lpszMainName))
//				lpszMainName = NULL;

			AddGlobalType(pNewType);

			pNewFunc = CreateFunction(m_dwCurrentFile);
			pNewFunc->pType = pNewType;
			pNewFunc->dwTypeAliasId = dwTypeId;
			pNewFunc->lpName = lpszMainName;
			lpszMainName = NULL;
			pNewType = NULL;

			AddFunction(pNewFunc);

			lpStabStr = tmp;
			break;
		default:
			throw SYMTAB_EXCEPTION_FIXME;
		}
	}
	catch (int a)
	{
		DestroyFunction(pNewFunc);
		DestroyCString(lpszMainName);
		DestroyGlobalType(pNewType);
		throw a;
	}
	catch (...)
	{
		DestroyFunction(pNewFunc);
		DestroyCString(lpszMainName);
		DestroyGlobalType(pNewType);
		throw SYMTAB_EXCEPTION_UNKNOWN;
	}
	
	DestroyCString(lpszMainName);
	DestroyGlobalType(pNewType);
	return pNewFunc;
}