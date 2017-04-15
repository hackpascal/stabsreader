#include "stdafx.h"

#include <stack>

#include "STABSymtab.h"

// ָ����һ���ַ��� c�������ʽ����
#define NEXT_CHAR(c) \
	if (*tmp != (c)) throw SYMTAB_EXCEPTION_BAD_FORMAT;

// ָ����һ���ַ��� c���������������ʽ����
#define NEXT_CHAR_INC(c) \
	if (*tmp != (c)) throw SYMTAB_EXCEPTION_BAD_FORMAT; \
	else ++tmp;

// �����һ���ַ��� c�������
#define OPT_CHAR_INC(c) \
	if (*tmp == (c)) ++tmp;

// ����һ���ַ�
#define SKIP_CHAR \
	++tmp;

// ����ָ���ַ��������ʽ����
#define JUMP_CHAR(c) \
	p = strchr(tmp, (c)); \
	if (!p) throw SYMTAB_EXCEPTION_BAD_FORMAT; \
	else tmp = p;

// ����ָ���ַ����������������ʽ����
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
			case N_SO:	// ��ȡ��ǰԴ�����ļ���·��
				VALIDATE_STRING_ELSE(m_pStabs[it].n_strx)
				{
					// ������һ��Դ�����ļ��ѽ���
					sSourcePath = "";
					m_dwCurrentFile = NONE;
					break;
				}

				lpszName = m_pStabStr + m_pStabs[it].n_strx;

				if (DirectoryExists(lpszName))
				{
					// �Ǹ�Ŀ¼
					sSourcePath = lpszName;
					for (size_t i = 0; i < sSourcePath.size(); i++)
						if (sSourcePath[i] == '/') sSourcePath[i] = '\\';
					bSourceIsFile = FALSE;
				}
				else
				{
					if (FileExists(lpszName))
					{
						// �Ǹ��ļ�
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
							// ���Ŀ¼�͵�ǰ�ַ���
							std::string sTemp(sSourcePath);
							if (sTemp.back() != '\\') sTemp.push_back('\\');
							sTemp.append(lpszName);

							// �ж��Ƿ����ļ�
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
								// ʲô������
								sSourcePath.clear();
								bSourceIsFile = FALSE;
							}
						}
					}
				}

				break;

			case N_LSYM: // ȫ������ �����ľֲ�����
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
			case N_RSYM: // �Ĵ�������/����
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
			case N_STSYM: // ȫ�ֱ��� (�ѳ�ʼ������̬)
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				pVar = ResolveVariable(lpszName);
				pVar->llAddress = m_pStabs[it].n_value;
				break;
			case N_LCSYM: // ȫ�ֱ��� (δʼ��)
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				pVar = ResolveVariable(lpszName);
				pVar->llAddress = m_pStabs[it].n_value;
				break;
			case N_GSYM: // ȫ�ֱ��� (�ѳ�ʼ�������ڷ��ű���ҵ�ַ)
				VALIDATE_STRING(m_pStabs[it].n_strx);
				lpszName = m_pStabStr + m_pStabs[it].n_strx;
				pVar = ResolveVariable(lpszName);
				break;
			case N_FUN: // ���� �ȿ����ǿ�ʼ��Ҳ�����ǽ�������Ҫ�� n_strx �ж�
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
			case N_SLINE: // �������к���Ϣ
				if (pCurrentFunc)
				{
					AddFunctionLineNumber(pCurrentFunc, m_pStabs[it].n_desc, m_pStabs[it].n_value);
				}
				break;
			case N_PSYM: // �����Ĳ���
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
			case N_LBRAC: // ������ʼλ��
				if (m_pStabs[it].n_value)
					if (BlockLayers.size())
					{
						PCODE_BLOCK pNewBlock = CreateCodeBlock(BlockLayers.top());
						pNewBlock->dwStartOffset = m_pStabs[it].n_value;
						BlockLayers.push(pNewBlock);
					}
				break;
			case N_RBRAC: // �������λ��
				if (BlockLayers.size())
				{
					PCODE_BLOCK pCodeBlock = BlockLayers.top();
					BlockLayers.pop();
					pCodeBlock->dwSize = m_pStabs[it].n_value - pCodeBlock->dwStartOffset + 1;
				}
				break;
			case N_BNSYM: // ������ʼ����һ����־
				break;
			case N_ENSYM: // ������������һ����־
				pCurrentFunc = NULL;
				pFuncProtoType = NULL;
				break;

			case N_UNDF:// δ����
			case N_SOL: // #include ���ļ�
			case N_OPT: // ��־��GCC �б�ʾ���ļ��� gcc ����
			case N_BINCL: // #include ��ʼ
			case N_EINCL: // #include ����
			case N_EXCL: // ��ɾ���� #include ռλ�� (�� #include ���б���)
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
				_tprintf(_T("δ֪�쳣\n"));
				break;
			case SYMTAB_EXCEPTION_FIXME:
				_tprintf(_T("FIXME �쳣\n"));
				break;
			case SYMTAB_EXCEPTION_BAD_ALLOCATION:
				_tprintf(_T("�ڴ����ʧ��\n"));
				break;
			case SYMTAB_EXCEPTION_BAD_FORMAT:
				_tprintf(_T("��ʽ����\n"));
				break;
			case SYMTAB_EXCEPTION_UNKNOWN_TYPE:
				_tprintf(_T("δ֪����\n"));
				break;
			case SYMTAB_EXCEPTION_UNKNOWN_RANGE:
				_tprintf(_T("δ֪�ӽ緶Χ\n"));
				break;
			}
		}
		//*/
		it++;
	}

	//fclose(f);

	return TRUE;
}

// ���� STABS �������ƵĽ���λ��
LPSTR CStabSymtab::GetEndOfName(LPSTR lpStr)
{
	// ��û������ʱ
	if (*lpStr == '(')
		return lpStr;
	if (*lpStr == '*')
		return lpStr;
	if (*lpStr == ',')
		return lpStr;

	LPSTR p = strchr(lpStr, ':');
	if (!p) return lpStr;
	if (p == lpStr) return lpStr;
	
	// ����ð�� (::) �� C++ �е����з���
	while (p && p[1] == ':')
	{
		p = strchr(p + 2, ':');
		if (!p) return lpStr;
	};
	return p;
}

// ������ʱ������
LPTSTR CStabSymtab::DumpName()
{
	TCHAR pszName[255];
	LPTSTR lpszFinalName;
		
	// ʹ�� __unnamed__<num>__ ��ʽ������
	*pszName = 0;
	_stprintf_s(pszName, 255, _T("__unnamed__%x__"), m_nUnnamedIndex++);

	lpszFinalName = new TCHAR[_tcslen(pszName) + 1];
	StringCchCopy(lpszFinalName, _tcslen(pszName) + 1, pszName);

	return lpszFinalName;
}

// ��ȡ����/����/����/��������������ת��Ϊ���ʵı���
LPTSTR CStabSymtab::DumpName(LPSTR &lpName, BOOL bNoAnonymous)
{
	LPSTR p, lpszTemp;
	LPTSTR lpszFinalName;
	int cchSize;

	p = GetEndOfName(lpName);

	if (p == lpName)
	{
		// ʹ����ʱ������
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

// ��ȡ���͵�Ψһ��ʶ��
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
	// ��ͬ�� MAKEDWORD(wFileId, wTypeId)
	// ����� wFileId ��ͬ�� m_cCurrentFile
	return (wFileId << 16) | (wTypeId & 0xffff);
}

// �����ӽ�����ȷ��������������
ENUM_BASIC_TYPE CStabSymtab::GetBasicType(LPSTR &lpSubrange)
{

#define LBOUND(t) \
	{ Result = (t); goto LBound; }
#define UBOUND(t) \
	{ Result = (t); goto UBound; }

	ENUM_BASIC_TYPE Result;
	LPSTR p;

	// ����д��Ϊ�˱�������ַ��������ֵ�ת��
	// ͬʱҲ��Ϊ��͵��
	if (!strncmp(lpSubrange, "-128;", 5) || !strncmp(lpSubrange, "0200;", 5))
		LBOUND(Char);
	if (!strncmp(lpSubrange, "-32768;", 7) || !strncmp(lpSubrange, "0100000;", 8))
		LBOUND(Short);
	if (!strncmp(lpSubrange, "-2147483648;", 12) || !strncmp(lpSubrange, "020000000000;", 13))
		LBOUND(Int);
	if (!strncmp(lpSubrange, "-9223372036854775808;", 21) || !strncmp(lpSubrange, "01000000000000000000000;", 24))
		LBOUND(Longlong);
	// ��������
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
		// ��ȡ�½�
		if (!(p = strchr(lpSubrange, ';')))
			throw SYMTAB_EXCEPTION_UNKNOWN_RANGE;

		lpSubrange = p + 1;

		// ������⣬�� 7 λ�� char ���͡��������� 8 λ�з��ŵ� char
		if (!strncmp(lpSubrange, "127;", 4))
			UBOUND(Char);
		// �޷�������
		if (!strncmp(lpSubrange, "255;", 4) || !strncmp(lpSubrange, "0377;", 5))
			UBOUND(Byte);
		if (!strncmp(lpSubrange, "65535;", 6) || !strncmp(lpSubrange, "0177777;", 8))
			UBOUND(Word);
		if (!strncmp(lpSubrange, "4294967295;", 11) || !strncmp(lpSubrange, "037777777777;", 13))
			UBOUND(Dword);
		if (!strncmp(lpSubrange, "18446744073709551615;", 21) || !strncmp(lpSubrange, "01777777777777777777777;", 24))
			UBOUND(Qword);

		// FIXME: û�п��ǵ�����Ч����
		throw SYMTAB_EXCEPTION_UNKNOWN_RANGE;
	}

	// FIXME: û�п��ǵ�����Ч����
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

// ȷ������/��������
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

// ��������
PGLOBAL_TYPE CStabSymtab::ResolveType(LPSTR &lpStabStr, DWORD dwLastId, LPTSTR lpszTypeName, BOOL bResolveVoid, BOOL bAddToList)
{
	LPSTR tmp = lpStabStr;
	ENUM_BASIC_TYPE BaseType, CurrentType;
	DWORD dwUniqueId;		// ��ǰ���͵� Id
	DWORD dwAliasId;
	DWORD dwLastArrayId;	// ��һ������ά�ȵ� Id
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
		case 't':	// [��һ��] �����Ƶ�����
		case 'T':
			SKIP_CHAR;

			if (*tmp == 't')
			{
				// C++ ���룬 struct class union �Դ� typedef ����
				SKIP_CHAR;
			}

			dwUniqueId = GetUniqueId(tmp);

			// ������һ���Ѵ��ڵ�����
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
			
			// FIXME: ��ʵ���鲻Ӧ��������������
			// STABS �ж�������ķ�ʽ��ͬ�� C/C++������������ Pascal ����ʽ�ṹ��
			// val: array[l1..h1] of array[l2..h2] of .. of array[ln..hn] of Type;
			// val:(x,y1)=a;l1;h1;(x,y2)=a;l2;h2;...(x,yn)=a;ln;hn;(Type)

			// �������ͣ���Ϊ���飬�򱣴�����ά�ȣ�����������
			pTempType = ResolveType(tmp, dwUniqueId, lpszMainName, bResolveVoid, TRUE);
			while (pTempType->BasicType == Array && pTempType->bNew)
			{
				// ���������ͣ���ʱֻ��������һά�����䱣�棬��������
				pArrayDims->push_back(pTempType->pDims->operator[](0));
				// �������� Id
				dwLastArrayId = pTempType->dwCurrentId;
				// ��������������Ϊ����ʱ�������Ͳ��ᱻ��ӵ������б���
				// ��˱���������
				DestroyGlobalType(pTempType);
				// ��������
				pTempType = ResolveType(tmp, dwUniqueId, lpszMainName, bResolveVoid, TRUE);
			}

			// �������ͣ���ʱ�� pFirstType �����������
			if (pArrayDims->size())
			{
				// �½���������
				pFirstType = CreateGlobalType(m_dwCurrentFile);
				pFirstType->BasicType = Array;
				pFirstType->pTypeTo = pTempType;
				pFirstType->dwTypeAliasTo = pTempType->dwMasterId;
				pFirstType->pDims = pArrayDims;
				AddGlobalType(pFirstType);
				pArrayDims = NULL;

				// FIXME: һ�� hack��������һά���� Id����ʹ֮��Ϊ�����͵ı���
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
		case '@':	// [��1.5��] ��ʱ�����Ǵ����ݣ�����
			SKIP_CHAR;
			NEXT_CHAR_INC('s');
			while (isdigit(*tmp)) tmp++;
			NEXT_CHAR_INC(';');

			pFirstType = ResolveType(tmp, dwLastId, _T(""), bResolveVoid, bAddToList);
			break;
		case 'r':	// [�ڶ���] Subrange, �ӽ����� (˵���˾��ǻ�������)
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
		case 'R':	// [�ڶ���] �ڽ���������
			SKIP_CHAR;

			if (!isdigit(*tmp)) throw SYMTAB_EXCEPTION_BAD_FORMAT;
			cFpType = *tmp;
			SKIP_CHAR;
			NEXT_CHAR_INC(';');

			BaseType = GetComplexType(tmp);

			// ��Щ���Ǹ�������
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
				// FIXME: STABS �ֲ��϶�û�е�����
				throw SYMTAB_EXCEPTION_FIXME;
			}

			pFirstType = CreateGlobalType(m_dwCurrentFile);
			pFirstType->BasicType = BaseType;

			if (bAddToList)
					AddGlobalType(pFirstType);
			break;
		case '-':	// [�ڶ���] ������ʶ����ʾ�ڽ�����
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
		case 'c':	// [�ڶ���] ����ʼֵ�ľ�̬ȫ�ֱ���
			// FIXME: ����û����������
			pFirstType = NULL;
			break;
		case 'B':	// [�ڶ���] volatile
			SKIP_CHAR;
		case '(':	// [�ڶ���] Void ���͡��������������Ƶ�����
			dwAliasId = GetUniqueId(tmp);
		
			// Void ����
			if (bResolveVoid && (dwAliasId == dwLastId))
			{
				pFirstType = CreateGlobalType(m_dwCurrentFile);
				pFirstType->BasicType = Void;
				AddGlobalTypeAlias(pFirstType, dwAliasId);

				if (bAddToList)
					AddGlobalType(pFirstType);

				break;
			}
		
			// ���ͱ���
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
					// �������ͣ�������� bNew Ϊ FALSE ��ʾ����������
					pFirstType->bNew = FALSE;

					if (pFirstType->dwFileId == dwFileId)
						pFirstType->dwCurrentId = dwAliasId;
					else
						pFirstType->dwCurrentId = pFirstType->dwMasterId;
					break;
				}

				// FIXME: �Ҳ������ͣ�Ĭ��Ϊ PVOID

				if (!(pFirstType = GetGlobalType(_T("PVOID"))))
					pFirstType = GetGlobalType(_T("int"));

				break;
			}

			// �����Ƶ�������
			NEXT_CHAR_INC('=');

			pFirstType = ResolveType(tmp, dwAliasId, _T(""), bResolveVoid, bAddToList);
			AddGlobalTypeAlias(pFirstType, dwAliasId);

			break;
		case 'k':	// [������] ��������
		case '*':	// [������] ָ������
		case 'x':	// [������] ǰ������
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
					// FIXME: ������������ô
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
		case 'f':	// [������] ����������ֻ��һ��Ψһ Id��û�в����ͷ�������
			SKIP_CHAR;
			// ���ú�����־λ
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
				// FIXME: ��������������Ǹ���ģ�
				SKIP_CHAR;
				pTempType = ResolveType(tmp, dwLastId, _T(""), FALSE, FALSE);
				DestroyGlobalType(pTempType);
			}

			break;
		case 'a':	// [������] ��������
			SKIP_CHAR;
			// ֻ����һ��ά�ȣ�Ȼ������һ������������϶�ά���飬
			// ���򽫻��������ʽ���������ͣ���
			// һά���� -> һά���� -> ... -> һά���� -> ��������
			// ����˵���μ���һ����˵��

			// 'a' ������� 'r'��˵������ķ�Χ������ Subrange ����
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
		case 's':	// [������] �ṹ��/����������
		case 'u':
			if (*tmp == 's') CurrentType = Structure;
			if (*tmp == 'u') CurrentType = Union;
			SKIP_CHAR;

			// �����������Ա���ʹ������ָ��ʱ�ܹ��ҵ�����
			if (!(pFirstType = GetGlobalType(dwLastId, m_dwCurrentFile)))
			{
				LPTSTR lpszTemp;
				pFirstType = CreateGlobalType(m_dwCurrentFile);

				if (lpszTypeName == NULL || *lpszTypeName == 0)
					lpszTemp = NULL; // �ṹ/��������������
				else
					lpszTemp = _tcsdup(lpszTypeName);

				if (AddGlobalTypeAlias(pFirstType, dwLastId, lpszTemp))
					lpszTemp = NULL;

				AddGlobalType(pFirstType);
				DestroyCString(lpszTemp);
			}

			pFirstType->BasicType = CurrentType;

			// �����ʼ�����֣���������ǽṹ��/��������ֽ���
			while (isdigit(*tmp)) SKIP_CHAR;

			pMembers = new SYMTAB_VARIABLE_LIST;

			while (*tmp != ';')
			{
				cFpType = '2';
				// ���Ա�ɼ������η�
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
		case 'e':	// [������] ö������
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
			// FIXME: û�п��ǵ�������
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

// ��������
PVARIABLE CStabSymtab::ResolveVariable(LPSTR &lpStabStr, BOOL bResolveName, BOOL bAddToList)
{
	LPSTR tmp = lpStabStr;
	DWORD dwTypeId;		// ���������� Id
	DWORD dwLastArrayId;	// ��һ������ά�ȵ� Id
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
		case 'G':	// ȫ�ֱ��������ڷ��ű��в��Ҿ��Ե�ַ
		case 'S':	// ȫ�־�̬����
		case 'V':	// ������̬����
		case 'p':	// ��������
		case 'r':	// �Ĵ�������/����
		case 'P':	// �Ĵ�������
			if (*tmp == 'G') pNewVar->Type = Global;
			if (*tmp == 'S') pNewVar->Type = GlobalStatic;
			if (*tmp == 'V') pNewVar->Type = LocalStatic;
			if (*tmp == 'p') pNewVar->Type = Parameter;
			if (*tmp == 'r') pNewVar->Type = LocalRegister;
			if (*tmp == 'P') pNewVar->Type = ParameterRegister;
			SKIP_CHAR;
		case '(':	// ����
			pArrayDims = new SYMTAB_ARRAY_DIMS;				
			dwLastArrayId = 0;
			dwTypeId = 0;

			// bResolveVoid ������������Ϊ FALSE, �Է�ֹ�������ͽ���Ϊ void
			pTempType = ResolveType(tmp, 0, _T(""), FALSE, TRUE);
			while (pTempType->BasicType == Array && pTempType->bNew)
			{
				// ���������ͣ���ʱֻ��������һά�����䱣�棬��������
				pArrayDims->push_back(pTempType->pDims->operator[](0));
				// ��������� Id
				if (!dwTypeId) dwTypeId = pTempType->dwCurrentId;
				dwLastArrayId = pTempType->dwCurrentId;
				// ��������������Ϊ����ʱ�������Ͳ��ᱻ��ӵ������б���
				// ��˱���������
				DestroyGlobalType(pTempType);
				pTempType = ResolveType(tmp, 0, _T(""), FALSE, TRUE);
			}

			// �������ͣ���ʱ�� pFirstType ������ָ�������
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

				// FIXME: һ�� hack��ʹһά�����ͬ��ʹ�ô����������
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
			// FIXME: δ���ǵ�������
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

// ��������
PFUNCTION CStabSymtab::ResolveFunction(LPSTR &lpStabStr)
{
	LPSTR tmp = lpStabStr;
	DWORD dwTypeId;		// ���������� Id
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

			// ��������ԭ��
			if (!(pNewType = GetGlobalType(dwTypeId, m_dwCurrentFile)))
			{
				pNewType = CreateGlobalType(m_dwCurrentFile);
				pNewType->BasicType = Function;
			}

			// ���ú�����  // FIXME: ����ԭ�����Ͳ�Ӧ��������
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