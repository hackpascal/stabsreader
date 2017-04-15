
#include "stdafx.h"

#include <algorithm>

#include "BaseSymtab.h"

/* 将路径转换为 DOS 格式 */
#define PATH_TO_DOS(s) \
	for (size_t i = 0; i < strlen(s); i++) \
		if ((s)[i] == '/') \
			(s)[i] = '\\';

CBaseSymtab::CBaseSymtab()
{
	m_b64Bit = FALSE;
	m_cFiles = new SYMTAB_STRING_LIST;
	m_cTypes = new SYMTAB_TYPE_LIST;
	m_cVariables = new SYMTAB_VARIABLE_LIST;
	m_cFunctions = new SYMTAB_FUNCTION_LIST;
}

CBaseSymtab::~CBaseSymtab()
{
	for (size_t i = 0; i < m_cFiles->size(); i++)
		DestroyCString(m_cFiles->operator[](i));
	delete m_cFiles;

	for (size_t i = 0; i < m_cTypes->size(); i++)
		DestroyGlobalType(m_cTypes->operator[](i));
	delete m_cTypes;

	for (size_t i = 0; i < m_cVariables->size(); i++)
		DestroyVariable(m_cVariables->operator[](i));
	delete m_cVariables;

	for (size_t i = 0; i < m_cFunctions->size(); i++)
		DestroyFunction(m_cFunctions->operator[](i));
	delete m_cFunctions;
}

// 判断一个路径是否指向一个文件夹
BOOL CBaseSymtab::DirectoryExists(LPCSTR lpPath)
{
	WIN32_FIND_DATAA wfd;
	BOOL bResult = FALSE;
	HANDLE hFind;
	LPSTR lpszPath = new CHAR[strlen(lpPath) + 1];

	StringCchCopyA(lpszPath, strlen(lpPath) + 1, lpPath);
	PATH_TO_DOS(lpszPath);
	if (lpszPath[strlen(lpPath) - 1] == '\\') lpszPath[strlen(lpPath) - 1] = 0;

	hFind = FindFirstFileA(lpszPath, &wfd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		bResult = wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY; 
		FindClose(hFind);
	}
	else
	{
		if(strlen(lpszPath) == 3 && lpszPath[1] == ':' && (lpszPath[2] == '/' || lpszPath[2] == ':'))
			bResult = TRUE; 
	}

	delete [] lpszPath;

	return bResult;
}

// 判断一个路径是否指向一个文件
BOOL CBaseSymtab::FileExists(LPCSTR lpPath)
{
	WIN32_FIND_DATAA wfd;
	BOOL bResult = FALSE;
	HANDLE hFind;

	hFind = FindFirstFileA(lpPath, &wfd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		bResult = !(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); 
		FindClose(hFind);
	}
	return bResult;
}

size_t CBaseSymtab::SplitString(LPCSTR lpString, LPCSTR lpSplit, LPSTR lpResult[], size_t nCountOfResultArray)
{
	size_t nStrings = 0;
	LPSTR s = const_cast<LPSTR>(lpString);
	char *p, *t;

	p = strstr (s, lpSplit);
	while (p)
	{
		if (nStrings + 1 >= nCountOfResultArray)
			goto finish;

		t = lpResult[nStrings++] = new CHAR[static_cast<size_t>(p - s) + 1];
		StringCchCopyNA(t, static_cast<size_t>(p - s) + 1, s, static_cast<size_t>(p - s));

		s = p + strlen (lpSplit);
		p = strstr (s, lpSplit);
	}

	if (nStrings + 1 > nCountOfResultArray)
		goto finish;

	t = lpResult[nStrings++] = new CHAR[strlen(s) + 1];
	StringCchCopyA (t, strlen(s) + 1, s);

finish:

	return nStrings;
}

BOOL CBaseSymtab::PathCanonicalize(LPSTR lpPath)
{
	LPSTR lpszTemp, tmp, lpszFinal;
	LPSTR *lpElements, *lpResult;
	size_t nElemCount, nResultCount;
	size_t len;
	BOOL bResult;

	lpszTemp = new CHAR[strlen (lpPath) + 1];
	StringCchCopyA (lpszTemp, strlen (lpPath) + 1, lpPath);
	PATH_TO_DOS (lpszTemp);

	nElemCount = 1;
	for (size_t i = 0; i < strlen (lpszTemp); i++) if (lpszTemp[i] == '\\') nElemCount++;
	lpElements = new LPSTR[nElemCount];
	lpResult = new LPSTR[nElemCount];
	nElemCount = SplitString (lpszTemp, "\\", lpElements, nElemCount);

	len = 0;
	nResultCount = 0;
	for (size_t i = 0; i < nElemCount; i++)
	{
		if (strcmp(lpElements[i], ".") && strcmp(lpElements[i], ".."))
		{
			lpResult[nResultCount++] = lpElements[i];
			len += strlen(lpElements[i]);
		}
		else
		{
			if (!strcmp(lpElements[i], "..") && nResultCount)
			{
				tmp = lpResult[nResultCount - 1];
				if (!(nResultCount == 1 && tmp[0] == 0) && !(isalpha(tmp[0]) && tmp[1] == ':'))
				{
					len -= strlen(tmp);
					nResultCount--;
				}
			}
		}
	}

	len += nResultCount + 1;
	lpszFinal = new CHAR[len];
	lpszFinal[0] = 0;

	for (size_t i = 0; i < nResultCount - 1; i++)
	{
		StringCchCatA(lpszFinal, len, lpResult[i]);
		StringCchCatA(lpszFinal, len, "\\");
	}
	StringCchCatA(lpszFinal, len, lpResult[nResultCount - 1]);

	if (strlen(lpszFinal) <= strlen(lpPath))
	{
		StringCchCopyA(lpPath, strlen(lpPath) + 1, lpszFinal);
		bResult = TRUE;
	}
	else
		bResult = FALSE;

	for (size_t i = 0; i < nElemCount; i++)
		delete [] lpElements[i];
	delete [] lpElements;
	delete [] lpResult;
	delete [] lpszFinal;
	delete [] lpszTemp;

	return bResult;
}

size_t CBaseSymtab::AddSourceFile(LPCSTR lpFileName)
{
	LPSTR lpszTemp;

	lpszTemp = new CHAR[strlen(lpFileName) + 1];
	StringCchCopyA(lpszTemp, strlen(lpFileName) + 1, lpFileName);
	PathCanonicalize(lpszTemp);

#ifdef UNICODE
	int cchSize = MultiByteToWideChar(CP_ACP, 0, lpszTemp, -1, NULL,  NULL);
	LPWSTR lpszFile = new WCHAR[cchSize];
	MultiByteToWideChar(CP_ACP, 0, lpszTemp, -1, lpszFile,  cchSize);
#else
	LPSTR lpszFile = lpszTemp;
#endif

	m_cFiles->push_back(lpszFile);

#ifdef UNICODE
	delete [] lpszTemp;
#endif

	return m_cFiles->size() - 1;
}

LPCTSTR CBaseSymtab::GetSourceFile(DWORD dwIndex)
{
	if (dwIndex >= m_cFiles->size())
		return NULL;

	return m_cFiles->operator[](dwIndex);
}

PVARIABLE CBaseSymtab::CreateVariable(DWORD dwFileId)
{
	PVARIABLE pNewVar = NULL;

	try
	{
		pNewVar = new VARIABLE;
		pNewVar->llAddress = 0;
		pNewVar->dwBitLength = 0;
		pNewVar->dwBitStart = 0;
		pNewVar->dwFileId = dwFileId;
		pNewVar->dwTypeAliasId = 0;
		pNewVar->pType = NULL;
		pNewVar->dwRegisterNum = 0;
		pNewVar->lpName = NULL;
		pNewVar->Visibility = Public;
		pNewVar->wLineNum = 0;
		pNewVar->Type = Local;
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}
	
	return pNewVar;
}

PVARIABLE CBaseSymtab::AddVariable(PVARIABLE pVariable)
{
	try
	{
		m_cVariables->push_back(pVariable);
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}
	return pVariable;
}

PVARIABLE CBaseSymtab::GetVariable(LPCTSTR lpName, BOOL bIgnoreCase, DWORD dwFileId)
{
	SYMTAB_VARIABLE_LIST::iterator i;
	BOOL bCompareResult;
	for (i = m_cVariables->begin(); i != m_cVariables->end(); ++i)
	{
		if ((*i)->dwFileId != dwFileId && dwFileId != NONE) continue;
		if (bIgnoreCase)
			bCompareResult = _tcsicmp(lpName, (*i)->lpName) == 0;
		else
			bCompareResult = _tcscmp(lpName, (*i)->lpName) == 0;

		if (bCompareResult)
			return *i;
	}
	return NULL;
}

PVARIABLE CBaseSymtab::GetVariable(INT64 lpAddress, DWORD dwFileId)
{
	SYMTAB_VARIABLE_LIST::iterator i;
	DWORD dwTypeSize;
	for (i = m_cVariables->begin(); i != m_cVariables->end(); ++i)
	{
		if ((*i)->dwFileId != dwFileId && dwFileId != NONE) continue;
		dwTypeSize = CalcTypeSize((*i)->pType);
		if (!dwTypeSize) continue; // 这个变量的类型不合法

		if ((*i)->llAddress <= lpAddress && lpAddress < (*i)->llAddress + dwTypeSize)
			return *i;
	}
	return NULL;
}

VOID CBaseSymtab::DestroyVariable(PVARIABLE &pVariable)
{
	if (!pVariable) return;

	DestroyCString(pVariable->lpName);

	delete pVariable;
	pVariable = NULL;
}

VOID CBaseSymtab::DestroyMembers(PSYMTAB_VARIABLE_LIST &pMembers)
{
	if (!pMembers) return;

	SYMTAB_VARIABLE_LIST::iterator i;
	for (i = pMembers->begin(); i != pMembers->end(); ++i)
		DestroyVariable(*i);

	delete pMembers;
	pMembers = NULL;
}

VOID CBaseSymtab::DestroyArrayDims(PSYMTAB_ARRAY_DIMS &pArrayDims)
{
	if (!pArrayDims) return;

	delete pArrayDims;
	pArrayDims = NULL;
}

VOID CBaseSymtab::DestroyTypeNames(PSYMTAB_TYPE_NAME_LIST &pNames)
{
	SYMTAB_TYPE_NAME_LIST::iterator i;

	if (!pNames) return;

	for (i = pNames->begin(); i != pNames->end(); ++i)
		delete [] i->second;

	delete pNames;
	pNames = NULL;
}

VOID CBaseSymtab::DestroyTypeIds(PSYMTAB_ID_LIST &pIds)
{
	if (!pIds) return;

	delete pIds;
	pIds = NULL;
}

VOID CBaseSymtab::DestroyFunctionLines(PSYMTAB_FUNCTION_LINE_LIST &pLines)
{
	if (!pLines) return;

	delete pLines;
	pLines = NULL;
}

LPCTSTR CBaseSymtab::GetTypeName(DWORD dwId, DWORD dwFileId)
{
	SYMTAB_TYPE_LIST::iterator i;
	for (i = m_cTypes->begin(); i != m_cTypes->end(); ++i)
	{
		if ((*i)->dwFileId != dwFileId && dwFileId != NONE) continue;
		SYMTAB_TYPE_NAME_LIST::iterator j;
		j = (*i)->pNames->find(dwId);

		if (j != (*i)->pNames->end())
			return (*i)->pNames->begin()->second;
	}
	return _T("");
}

LPCTSTR CBaseSymtab::GetTypeName(PGLOBAL_TYPE pType, DWORD dwTypeAliasId)
{
	if (!pType) return _T("");

	SYMTAB_TYPE_NAME_LIST::iterator i = pType->pNames->find(dwTypeAliasId ? dwTypeAliasId : pType->dwMasterId);

	if (i != pType->pNames->end())
		return i->second;

	return _T("");
}

PGLOBAL_TYPE CBaseSymtab::GetGlobalType(DWORD dwTypeId, DWORD dwFileId)
{
	SYMTAB_TYPE_LIST::iterator i;
	for (i = m_cTypes->begin(); i != m_cTypes->end(); ++i)
	{
		if ((*i)->dwFileId != dwFileId && dwFileId != NONE) continue;

		if (std::binary_search((*i)->pIds->begin(), (*i)->pIds->end(), dwTypeId))
		{
			(*i)->bNew = FALSE;
			return *i;
		}
	}
	return NULL;
}

PGLOBAL_TYPE CBaseSymtab::GetGlobalType(LPCTSTR lpName, DWORD dwFileId, BOOL bIgnoreCase)
{
	SYMTAB_TYPE_LIST::iterator i;
	BOOL bCompareResult;
	for (i = m_cTypes->begin(); i != m_cTypes->end(); ++i)
	{
		if ((*i)->dwFileId != dwFileId && dwFileId != NONE) continue;
		SYMTAB_TYPE_NAME_LIST::iterator j;

		for (j = (*i)->pNames->begin(); j != (*i)->pNames->end(); ++j)
		{
			if (bIgnoreCase)
				bCompareResult = _tcsicmp(j->second, lpName) == 0;
			else
				bCompareResult = _tcscmp(j->second, lpName) == 0;

			if (bCompareResult)
				return *i;
		}
	}
	return NULL;
}

PGLOBAL_TYPE CBaseSymtab::CreateGlobalType(DWORD dwFileId)
{
	PGLOBAL_TYPE pNewType;
	try
	{
		pNewType = new GLOBAL_TYPE;
		pNewType->pNames = new SYMTAB_TYPE_NAME_LIST;
		pNewType->pIds = new SYMTAB_ID_LIST;
		pNewType->dwMasterId = 0;
		pNewType->dwCurrentId = 0;
		pNewType->bNew = TRUE;
		pNewType->BasicType = None;
		pNewType->dwFileId = dwFileId;
		pNewType->dwTypeAliasTo = 0;
		pNewType->pTypeTo = NULL;
		pNewType->pDims = NULL;
		pNewType->pMembers = NULL;
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}

	return pNewType;
}

PGLOBAL_TYPE CBaseSymtab::AddGlobalType(PGLOBAL_TYPE pGlobalType)
{
	try
	{
		if (std::find(m_cTypes->begin(), m_cTypes->end(), pGlobalType) == m_cTypes->end())
			m_cTypes->push_back(pGlobalType);
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}
	return pGlobalType;
}

BOOL CBaseSymtab::AddGlobalTypeAlias(PGLOBAL_TYPE pTypeCon, DWORD dwTypeId, LPTSTR lpName)
{
	if (lpName)
	{
		SYMTAB_TYPE_NAME_LIST::iterator i;
		i = pTypeCon->pNames->find(dwTypeId);
		if (i != pTypeCon->pNames->end())
		{
			if (!_tcscmp(i->second, lpName))
				return FALSE;
			else
			{
				DestroyCString(i->second);
				i->second = lpName;
				return TRUE;
			}
		}

		pTypeCon->pNames->insert(SYMTAB_TYPE_NAME_PAIR(dwTypeId, lpName));
	}

	if (!std::binary_search(pTypeCon->pIds->begin(), pTypeCon->pIds->end(), dwTypeId))
	{
		pTypeCon->pIds->push_back(dwTypeId);
		std::sort(pTypeCon->pIds->begin(), pTypeCon->pIds->end());
	}

	pTypeCon->dwCurrentId = dwTypeId;
	return TRUE;
}

VOID CBaseSymtab::DestroyGlobalType(PGLOBAL_TYPE &pGlobalType)
{
	if (!pGlobalType) return;

	DestroyMembers(pGlobalType->pMembers);
	DestroyArrayDims(pGlobalType->pDims);
	DestroyTypeNames(pGlobalType->pNames);
	DestroyTypeIds(pGlobalType->pIds);

	delete pGlobalType;

	pGlobalType = NULL;
}

PCODE_BLOCK CBaseSymtab::CreateCodeBlock(PCODE_BLOCK pCurrentCodeBlock)
{
	PCODE_BLOCK pNewBlock = NULL;
	
	try
	{
		pNewBlock = new CODE_BLOCK;
		pNewBlock->dwSize = 0;
		pNewBlock->dwStartOffset = 0;
		pNewBlock->pInnerBlocks = new std::vector<PCODE_BLOCK>;
		pNewBlock->pVars = new SYMTAB_VARIABLE_LIST;

		if (pCurrentCodeBlock) pCurrentCodeBlock->pInnerBlocks->push_back(pNewBlock);
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}

	return pNewBlock;
}

BOOL CBaseSymtab::AddCodeBlockVariable(PCODE_BLOCK pCodeBlock, PVARIABLE pVar)
{
	if (!pCodeBlock || !pVar) return FALSE;

	try
	{
		pCodeBlock->pVars->push_back(pVar);
		return TRUE;
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}

	return FALSE;
}

VOID CBaseSymtab::DestroyCodeBlock(PCODE_BLOCK &pCodeBlock)
{
	if (!pCodeBlock) return;

	DestroyMembers(pCodeBlock->pVars);

	if (pCodeBlock->pInnerBlocks)
	{
		std::vector<PCODE_BLOCK>::iterator i;
		for (i = pCodeBlock->pInnerBlocks->begin(); i != pCodeBlock->pInnerBlocks->end(); ++i)
			DestroyCodeBlock(*i);
		delete pCodeBlock->pInnerBlocks;
	}

	delete pCodeBlock;
	pCodeBlock = NULL;
}

PFUNCTION CBaseSymtab::CreateFunction(DWORD dwFileId)
{
	PFUNCTION pNewFunc = NULL;
	
	try
	{
		pNewFunc = new FUNCTION;
		pNewFunc->dwAddress = 0;
		pNewFunc->dwCodeSize = 0;
		pNewFunc->dwFileId = dwFileId;
		pNewFunc->dwTypeAliasId = 0;
		pNewFunc->pType = NULL;
		pNewFunc->lpName = NULL;
		pNewFunc->pLines = new SYMTAB_FUNCTION_LINE_LIST;
		pNewFunc->pFirstBlock = CreateCodeBlock();
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}

	return pNewFunc;
}

PFUNCTION CBaseSymtab::AddFunction(PFUNCTION pFunc)
{
	try
	{
		m_cFunctions->push_back(pFunc);
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}

	return pFunc;
}

PFUNCTION CBaseSymtab::GetFunction(LPTSTR lpName, BOOL bIgnoreCase, DWORD dwFileId)
{
	SYMTAB_FUNCTION_LIST::iterator i;
	BOOL bCompareResult;
	for (i = m_cFunctions->begin(); i != m_cFunctions->end(); ++i)
	{
		if ((*i)->dwFileId != dwFileId && dwFileId != NONE) continue;
		if (bIgnoreCase)
			bCompareResult = _tcsicmp(lpName, (*i)->lpName) == 0;
		else
			bCompareResult = _tcscmp(lpName, (*i)->lpName) == 0;

		if (bCompareResult)
			return *i;
	}
	return NULL;
}

BOOL CBaseSymtab::AddFunctionParameter(PGLOBAL_TYPE pFuncType, PVARIABLE pVar)
{
	if (!pFuncType || !pVar) return FALSE;

	try
	{
		if (!pFuncType->pMembers) pFuncType->pMembers = new SYMTAB_VARIABLE_LIST;
		pFuncType->pMembers->push_back(pVar);
		return TRUE;
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}

	return FALSE;
}

PVARIABLE CBaseSymtab::GetFunctionParameter(PGLOBAL_TYPE pFuncType, LPCTSTR lpName)
{
	SYMTAB_VARIABLE_LIST::iterator i;

	if (!pFuncType || !lpName || *lpName ==0) return NULL;
	if (!pFuncType->pMembers) return NULL;

	for (i = pFuncType->pMembers->begin(); i != pFuncType->pMembers->end(); ++i)
	{
		if (!_tcscmp((*i)->lpName, lpName))
			return *i;
	}

	return NULL;
}

BOOL CBaseSymtab::AddFunctionLineNumber(PFUNCTION pFunc, DWORD dwLineNum, DWORD dwOffset)
{
	LARGE_INTEGER llLineNum;

	if (!pFunc) return FALSE;

	try
	{
		llLineNum.LowPart = dwLineNum;
		llLineNum.HighPart = dwOffset;
		pFunc->pLines->push_back(llLineNum);
		return TRUE;
	}
	catch (std::bad_alloc)
	{
		throw SYMTAB_EXCEPTION_BAD_ALLOCATION;
	}

	return FALSE;
}

VOID CBaseSymtab::DestroyFunction(PFUNCTION &pFunc)
{
	if (!pFunc) return;

	DestroyCString(pFunc->lpName);
	DestroyFunctionLines(pFunc->pLines);
	DestroyCodeBlock(pFunc->pFirstBlock);

	delete pFunc;
	pFunc = NULL;
}

// 计算一个类型的大小
DWORD CBaseSymtab::CalcTypeSize(PGLOBAL_TYPE pType)
{
	DWORD dwElementNum;
	DWORD dwSize, dwMaxSize;
	LARGE_INTEGER llDim;

	if (!pType) return 0;

	switch (pType->BasicType)
	{
	case None: case Void: case Function: return 0;
	case Byte: case Char: return 1;
	case Short: case Word: return 2;
	case Int: case Dword: case Single: return 4;
	case Longlong: case Qword: case Double: case Complex: return 8;
	case LongDouble: return 12;
	case Extended: case Complex16: return 16;
	case Complex24: return 24;
	case Complex32: return 32;
	case Pointer: return m_b64Bit ? 8 : 4;
	case Enum: return 4; // FIXME: 默认 enum 为 unsigned long
	case Const:
		return CalcTypeSize(pType->pTypeTo);
	case Array:
		dwElementNum = 1;
		for (DWORD i = 0; i < pType->pDims->size(); i++)
		{
			llDim = pType->pDims->operator[](i);
			dwElementNum *= (llDim.HighPart - llDim.LowPart + 1);
		}
		return CalcTypeSize(pType->pTypeTo) * dwElementNum;
	case Structure: case Class:
		dwMaxSize = 0;
		dwSize = 0;
		if (!pType->pMembers) return dwMaxSize;
		for (DWORD i = 0; i < pType->pMembers->size(); i++)
		{
			PVARIABLE pVar = pType->pMembers->operator[](i);
			if (pVar->dwBitStart >= dwMaxSize)
			{
				dwMaxSize = pVar->dwBitStart;
				dwSize = pVar->dwBitLength;
			}
		}
		return (dwMaxSize + dwSize) / 8;
	case Union:
		dwMaxSize = 0;
		if (!pType->pMembers) return dwMaxSize;
		for (DWORD i = 0; i < pType->pMembers->size(); i++)
		{
			PVARIABLE pVar = pType->pMembers->operator[](i);
			if (pVar->dwBitLength >= dwMaxSize) dwMaxSize = pVar->dwBitLength;
		}
		return dwMaxSize / 8;
	default:
		return 0;
	}
}

// 计算一个类型的大小
DWORD CBaseSymtab::CalcTypeSize(DWORD dwTypeId, DWORD dwFileId)
{
	return CalcTypeSize(GetGlobalType(dwTypeId, dwFileId));
}


BOOL CBaseSymtab::GetTypeDefination(DWORD dwId, _tostringstream &s, BOOL bNameOnly, DWORD dwIndent, DWORD dwFileId)
{
	PGLOBAL_TYPE pType = GetGlobalType(dwId, dwFileId);	
	return GetTypeDefination(pType, dwId, s, bNameOnly, dwIndent);
}

BOOL CBaseSymtab::GetTypeDefination(PGLOBAL_TYPE pType, DWORD dwId, _tostringstream &s, BOOL bNameOnly, DWORD dwIndent)
{
	LPTSTR lpTypeName, lpAliasName;
	_tostringstream tss;
	_tstring ts;
	SYMTAB_TYPE_NAME_LIST::iterator master, alias;
	LARGE_INTEGER llDim;

	if (!pType) return FALSE;

	if (dwId != pType->dwMasterId)
	{
		// typedef master alias; 类型
		master = pType->pNames->find(pType->dwMasterId);
		alias = pType->pNames->find(dwId);

		if (!bNameOnly) s << _T("typedef ");

		if (alias != pType->pNames->end())
			lpAliasName = alias->second;
		else
			if (!bNameOnly)
				lpAliasName = _T("<unnamed>");
			else
				if (master != pType->pNames->end())
					lpAliasName = master->second;
				else
					lpAliasName = _T("<unnamed>");

		if (master != pType->pNames->end())
		{
			if (!bNameOnly)
			{
				switch (pType->BasicType)
				{
				case Structure:
					s << _T("struct ");
					break;
				case Class:
					s << _T("class ");
					break;
				case Union:
					s << _T("union ");
					break;
				case Enum:
					s << _T("enum ");
					break;
				}
				s << master->second << _T(" ");
			}
			s << lpAliasName;
		}
		else
		{
			switch (pType->BasicType)
			{
			case Void: s << _T("void ") << lpAliasName; break;
			case Byte: s << _T("unsigned char ") << lpAliasName; break;
			case Char: s << _T("char ") << lpAliasName; break;
			case Short: s << _T("short ") << lpAliasName; break;
			case Word: s << _T("unsigned short ") << lpAliasName; break;
			case Int: s << _T("int ") << lpAliasName; break;
			case Dword: s << _T("unsigned int ") << lpAliasName; break;
			case Longlong: s << _T("long long ") << lpAliasName; break;
			case Qword: s << _T("unsigned long long ") << lpAliasName; break;
			case Single: s << _T("float ") << lpAliasName; break;
			case Double: s << _T("double ") << lpAliasName; break;
			case LongDouble: s << _T("long double ") << lpAliasName; break;
			case Extended: s << _T("__Decimal128 ") << lpAliasName; break;
			case Complex: s << _T("__Complex ") << lpAliasName; break;
			case Complex16: s << _T("__Complex16 ") << lpAliasName; break;
			case Complex24: s << _T("__Complex24 ") << lpAliasName; break;
			case Complex32: s << _T("__Complex32 ") << lpAliasName; break;
			case Structure:
				s << _T("struct\n");
				s << _T("{\n");
				GetStructMember(pType, s, dwIndent);
				s << _T("} ") << lpAliasName;
				break;
			case Class:
				s << _T("class\n");
				s << _T("{\n");
				GetStructMember(pType, s, dwIndent);
				s << _T("} ") << lpAliasName;
				break;
			case Union:
				s << _T("union\n");
				s << _T("{\n");
				GetStructMember(pType, s, dwIndent);
				s << _T("} ") << lpAliasName;
				break;
			case Enum:
				s << _T("enum\n");
				s << _T("{\n");
				GetEnumMember(pType, s, dwIndent);
				s << _T("} ") << lpAliasName;
				break;
			case Const:
				s << _T("const ");
				if (!GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, s, TRUE, dwIndent))
					s << _T("<unknown>");
				s << _T(" ") << lpAliasName;
				break;
			case Pointer:
				if (!GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, s, TRUE, dwIndent))
					s << _T("<unknown>");
				s << _T(" *") << lpAliasName;
				break;
			case Array:
				if (!GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, s, TRUE, dwIndent))
					s << _T("<unknown>");
				s << _T(" ") << lpAliasName;
				for (size_t i = 0; i < pType->pDims->size(); i++)
				{
					llDim = pType->pDims->operator[](i);
					s << _T("[") << llDim.HighPart - llDim.LowPart + 1 << _T("]");
				}
				break;
			default:
				return FALSE;
			}			
		}
	}
	else
	{
		// struct union enum 函数定义
		master = pType->pNames->find(pType->dwMasterId);
		
		if (master == pType->pNames->end())
		{
			switch (pType->BasicType)
			{
			case Structure:
				tss.str(_T(""));
				tss << _T("\n");
				// 开头缩进
				for (DWORD i = 0; i < (dwIndent > 0 ? dwIndent - 1 : 0) * 2; i++) tss << _T(" ");
				tss << _T("{\n");
				GetStructMember(pType, tss, dwIndent > 0 ? dwIndent - 1 : 0);
				// 开头缩进
				for (DWORD i = 0; i < (dwIndent > 0 ? dwIndent - 1 : 0) * 2; i++) tss << _T(" ");
				tss << _T("}");
				ts = tss.str();
				lpTypeName = const_cast<LPTSTR>(ts.c_str());
				break;
			case Class:
				tss.str(_T(""));
				tss << _T("\n");
				// 开头缩进
				for (DWORD i = 0; i < (dwIndent > 0 ? dwIndent - 1 : 0) * 2; i++) tss << _T(" ");
				tss << _T("{\n");
				GetStructMember(pType, tss, dwIndent > 0 ? dwIndent - 1 : 0);
				// 开头缩进
				for (DWORD i = 0; i < (dwIndent > 0 ? dwIndent - 1 : 0) * 2; i++) tss << _T(" ");
				tss << _T("}");
				ts = tss.str();
				lpTypeName = const_cast<LPTSTR>(ts.c_str());
				break;
			case Union:
				tss.str(_T(""));
				tss << _T("\n");
				// 开头缩进
				for (DWORD i = 0; i < (dwIndent > 0 ? dwIndent - 1 : 0) * 2; i++) tss << _T(" ");
				tss << _T("{\n");
				GetStructMember(pType, tss, dwIndent > 0 ? dwIndent - 1 : 0);
				// 开头缩进
				for (DWORD i = 0; i < (dwIndent > 0 ? dwIndent - 1 : 0) * 2; i++) tss << _T(" ");
				tss << _T("}");
				ts = tss.str();
				lpTypeName = const_cast<LPTSTR>(ts.c_str());
				break;
			case Enum:
				tss.str(_T(""));
				tss << _T("\n");
				// 开头缩进
				for (DWORD i = 0; i < (dwIndent > 0 ? dwIndent - 1 : 0) * 2; i++) tss << _T(" ");
				tss << _T("{\n");
				GetEnumMember(pType, tss, dwIndent > 0 ? dwIndent - 1 : 0);
				// 开头缩进
				for (DWORD i = 0; i < (dwIndent > 0 ? dwIndent - 1 : 0) * 2; i++) tss << _T(" ");
				tss << _T("}");
				ts = tss.str();
				lpTypeName = const_cast<LPTSTR>(ts.c_str());
				break;
			case Pointer:
				tss.str(_T(""));
				if (!GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, tss, TRUE, dwIndent))
					tss << _T("<unknown>");
				tss << _T(" *");
				ts = tss.str();
				lpTypeName = const_cast<LPTSTR>(ts.c_str());
				break;
			case Const:
				tss.str(_T(""));
				tss << _T("const ");
				if (!GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, tss, TRUE, dwIndent))
					tss << _T("<unknown>");
				ts = tss.str();
				lpTypeName = const_cast<LPTSTR>(ts.c_str());
				break;
			default:
				lpTypeName = _T("<unnamed>");
			}
		}
		else
			lpTypeName = master->second;
		
		switch (pType->BasicType)
		{
		case Void: case Byte: case Char: case Short: case Word: case Int:
		case Dword: case Longlong: case Qword: case Single: case Double:
		case LongDouble: case Extended:
		case Complex: case Complex16: case Complex24: case Complex32:
			if (!bNameOnly) s << _T("内建类型 ");
			s << lpTypeName;
			break;
		case Structure:
			s << _T("struct ") << lpTypeName;
			if (!bNameOnly)
			{
				s << _T("\n");
				// 开头缩进
				for (DWORD i = 0; i < dwIndent * 2; i++) s << _T(" ");
				s << _T("{\n");
				GetStructMember(pType, s, dwIndent);
				// 开头缩进
				for (DWORD i = 0; i < dwIndent * 2; i++) s << _T(" ");
				s << _T("}");
			}
			break;
		case Class:
			s << _T("class ") << lpTypeName;
			if (!bNameOnly)
			{
				s << _T("\n");
				// 开头缩进
				for (DWORD i = 0; i < dwIndent * 2; i++) s << _T(" ");
				s << _T("{\n");
				GetStructMember(pType, s, dwIndent);
				// 开头缩进
				for (DWORD i = 0; i < dwIndent * 2; i++) s << _T(" ");
				s << _T("}");
			}
			break;
		case Union:
			s << _T("union ") << lpTypeName;
			if (!bNameOnly)
			{
				s << _T("\n");
				// 开头缩进
				for (DWORD i = 0; i < dwIndent * 2; i++) s << _T(" ");
				s << _T("{\n");
				GetStructMember(pType, s, dwIndent);
				// 开头缩进
				for (DWORD i = 0; i < dwIndent * 2; i++) s << _T(" ");
				s << _T("}");
			}
			break;
		case Enum:
			s << _T("enum ") << lpTypeName;
			if (!bNameOnly)
			{
				s << _T("\n");
				// 开头缩进
				for (DWORD i = 0; i < dwIndent * 2; i++) s << _T(" ");
				s << _T("{\n");
				GetEnumMember(pType, s, dwIndent);
				// 开头缩进
				for (DWORD i = 0; i < dwIndent * 2; i++) s << _T(" ");
				s << _T("}");
			}
			break;
		case Const:
			if (!bNameOnly)
			{
				s << _T("typedef const ");
				if (!GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, s, TRUE, dwIndent))
					s << _T("<unknown>");
				s << _T(" ");
			}
			s << lpTypeName;
			break;
		case Pointer:
			if (!bNameOnly)
			{
				s << _T("typedef ");
				// 指向函数的指针需要做特别处理
				if (pType->pTypeTo->BasicType != Function)
				{
					if (!GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, s, TRUE, dwIndent))
						s << _T("<unknown>");
					s << _T(" *");
					s << lpTypeName;
					if (lpTypeName[_tcslen(lpTypeName) - 1] != '*' && bNameOnly) s << _T(" ");
				}
				else
				{
					// 是函数
					// 返回值类型
					if (!GetTypeDefination(pType->pTypeTo->pTypeTo, pType->pTypeTo->dwTypeAliasTo, s, TRUE, dwIndent))
						s << _T("void");
					s << _T(" (*");
					// 
					s << lpTypeName;
					s << _T(")(");
					if (pType->pTypeTo->pMembers && pType->pTypeTo->pMembers->size())
					{
						// 有参数
						for (size_t i = 0; i < pType->pTypeTo->pMembers->size(); i++)
						{
							GetVariableDefination(pType->pTypeTo->pMembers->operator[](i), s, dwIndent);
						}
					}
					else
					{
						// 无参数
						s << _T("void");
					}
					s << _T(")");
				}
			}
			else
			{
				s << lpTypeName;
				if (lpTypeName[_tcslen(lpTypeName) - 1] != '*' && bNameOnly) s << _T(" ");
			}
			break;
		case Array:
			if (!GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, s, TRUE, dwIndent))
				s << _T("<unknown>");
			break;
		case Function:

			break;
		default:
			return FALSE;
		}
		
	}

	if (!bNameOnly) s << _T(";\n");

	return TRUE;
}

BOOL CBaseSymtab::GetStructMember(PGLOBAL_TYPE pType, _tostringstream &s, DWORD dwIndent)
{
	PVARIABLE pMember;

	if (!pType) return FALSE;
	if (pType->BasicType != Structure && pType->BasicType != Class && pType->BasicType != Union) return FALSE;

	if (pType->pMembers)
	{
		for (size_t i = 0; i < pType->pMembers->size(); i++)
		{
			// 开头缩进
			for (DWORD j = 0; j < dwIndent * 2; j++) s << _T(" ");
			s << _T("  ");
			
			pMember = pType->pMembers->operator[](i);
			GetVariableDefination(pMember, s, dwIndent + 1);

			s << _T(";");
			s << _T("\n");
		}
	}
	else
		s << _T("\n");

	return TRUE;
}
	
// 生成枚举成员
BOOL CBaseSymtab::GetEnumMember(PGLOBAL_TYPE pType, _tostringstream &s, DWORD dwIndent)
{
	PVARIABLE pMember;

	if (!pType) return FALSE;
	if (pType->BasicType != Enum) return FALSE;

	if (pType->pMembers)
	{
		for (size_t i = 0; i < pType->pMembers->size(); i++)
		{
			pMember = pType->pMembers->operator[](i);
			// 开头缩进
			for (DWORD j = 0; j < dwIndent * 2; j++) s << _T(" ");
			s << _T("  ");
			if (pMember->lpName) s << pMember->lpName << _T(" = ") << pMember->dwTypeAliasId;

			s << _T(";");
			s << _T("\n");
		}
	}
	else
		s << _T("\n");

	return TRUE;
}

// 生成变量定义
BOOL CBaseSymtab::GetVariableDefination(PVARIABLE pVar, _tostringstream &s, DWORD dwIndent)
{
	PGLOBAL_TYPE pVarType;
	LARGE_INTEGER llDim;

	if (!pVar) return FALSE;

	pVarType = pVar->pType;

	GetTypeDefination(pVarType, pVar->dwTypeAliasId, s, TRUE, dwIndent + 1);

	if (pVar->lpName && *pVar->lpName != 0)
	{
		if (pVarType->BasicType != Pointer)
			s << _T(" ");
		else
		{
			if (pVar->dwTypeAliasId != pVarType->dwMasterId)
				s << _T(" ");
		}
		s << pVar->lpName;
	}

	if (IsInt(pVarType->BasicType))
	{
		if (pVar->dwBitLength < TypeBitSize(pVarType->BasicType))
			s << _T(" :") << pVar->dwBitLength;
	}

	if (pVarType->BasicType == Array)
	{
		for (size_t i = 0; i < pVarType->pDims->size(); i++)
		{
			llDim = pVarType->pDims->operator[](i);
			s << _T("[") << llDim.HighPart - llDim.LowPart + 1 << _T("]");
		}
	}

	return TRUE;
}

VOID CBaseSymtab::PrintFunction(PFUNCTION pFunc)
{
	_tstring ts;
	_tostringstream s;
	PGLOBAL_TYPE pType;
	LARGE_INTEGER llLineNum;
	TCHAR lpszAddress[10];

	if (!pFunc) return;

	s.str(_T(""));
	
	pType = pFunc->pType;

	if (pType->pTypeTo)
		GetTypeDefination(pType->pTypeTo, pType->dwTypeAliasTo, s);
	else
		s<<_T("void");
	s<<_T(" ") << pFunc->lpName;
	
	s<<_T("(");
	if (pType->pMembers && pType->pMembers->size())
	{
		size_t i;
		for (i = 0; i < pType->pMembers->size() - 1; i++)
		{
			GetVariableDefination(pType->pMembers->operator[](i), s);
			s<<_T(", ");
		}
		GetVariableDefination(pType->pMembers->operator[](i), s);
	}
	else
		s<<_T("void");
	s<<_T(");");

	if (pFunc->pLines && pFunc->pLines->size())
	{
		s<<std::endl;
		for (size_t i =0; i < pFunc->pLines->size(); i++)
		{
			llLineNum = pFunc->pLines->operator[](i);
			StringCchPrintf(lpszAddress, 10, _T("%08lx"), llLineNum.HighPart + pFunc->dwAddress);
			s<<lpszAddress<<_T("   ");
			s<<llLineNum.LowPart<<std::endl;
		}
	}
	s<<std::endl;

	ts = s.str();
	_tcout << ts;
	_tcout.flush();
}