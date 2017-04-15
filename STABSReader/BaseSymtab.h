
#ifndef _BASESYMTAB_H_
#define _BASESYMTAB_H_

#include <stdexcept>
#include <sstream>
#include <vector>
#include <map>

#define NONE 0xffffffffUL

typedef enum
{
	Global,
	GlobalStatic,
	Local,
	LocalStatic,
	LocalRegister,
	Member,
	Parameter,
	ParameterRegister
} ENUM_VARIABLE_TYPE;

typedef enum
{
	None,
	Void,
	Byte,
	Char,
	Short,
	Word,
	Int,
	Dword,
	Longlong,
	Qword,
	Single,
	Double,
	LongDouble,
	Extended,
	Complex,
	Complex16,
	Complex24,
	Complex32,
	Structure,
	Class,
	Union,
	Enum,
	Const,
	Pointer,
	Array,
	Function
} ENUM_BASIC_TYPE;

typedef enum
{
	Private = 0,
	Protected = 1,
	Public = 2
} ENUM_MEMBER_VISIBILITY;

typedef std::vector<LPTSTR> SYMTAB_STRING_LIST, *PSYMTAB_STRING_LIST;
typedef std::vector<LARGE_INTEGER> SYMTAB_ARRAY_DIMS, *PSYMTAB_ARRAY_DIMS;
typedef std::map<DWORD, LPTSTR> SYMTAB_TYPE_NAME_LIST, *PSYMTAB_TYPE_NAME_LIST;
typedef std::pair<DWORD, LPTSTR> SYMTAB_TYPE_NAME_PAIR, *PSYMTAB_TYPE_NAME_PAIR;
typedef std::vector<LARGE_INTEGER> SYMTAB_FUNCTION_LINE_LIST, *PSYMTAB_FUNCTION_LINE_LIST;
typedef std::vector<DWORD> SYMTAB_ID_LIST, *PSYMTAB_ID_LIST;

struct _GLOBAL_TYPE;

typedef struct _VARIABLE
{
	LPTSTR lpName;				/* �������� */
	ENUM_VARIABLE_TYPE Type;	/* �������� */
	DWORD dwTypeAliasId;		/* �������������� Id */
	_GLOBAL_TYPE *pType;		/* �������������� */
	WORD wLineNum;				/* �������к� */
	ENUM_MEMBER_VISIBILITY Visibility;	/* ���Ա�ɼ��� */
	union
	{
		struct
		{
			DWORD dwFileId;		/* �����ڱ�ʾ����ʱʹ�� */
			DWORD dwBitStart;	/* �����ڽṹ/������ĳ�Ա�� */
			DWORD dwBitLength;	/* �����ڽṹ/������ĳ�Ա�� */
		};
		struct
		{
			INT64 llAddress;	/* ������ַ��ȫ��/��̬������ʾ���Ե�ַ���ֲ�����/������ʾ��Ե�ַ */
			DWORD dwRegisterNum;/* �Ĵ����š������ڼĴ��������ͼĴ������� */
		};
	};
} VARIABLE, *PVARIABLE;
typedef std::vector<PVARIABLE> SYMTAB_VARIABLE_LIST, *PSYMTAB_VARIABLE_LIST;

typedef struct _CODE_BLOCK
{
	DWORD dwStartOffset;
	DWORD dwSize;
	PSYMTAB_VARIABLE_LIST pVars;		/* �����ı��� */
	std::vector<_CODE_BLOCK *> *pInnerBlocks;	/* �ڲ����� */
} CODE_BLOCK, *PCODE_BLOCK;

typedef struct _FUNCTION
{
	LPTSTR lpName;						/* �������� */
	DWORD dwFileId;						/* ������λ�ڵ��ļ� Id */
	DWORD dwTypeAliasId;				/* ����ԭ�͵����� Id */
	_GLOBAL_TYPE *pType;				/* ����ԭ�͵����� */
	INT64 dwAddress;					/* ������ַ */
	DWORD dwCodeSize;					/* �����Ĵ����С */
	
	PSYMTAB_FUNCTION_LINE_LIST pLines;	/* �������к���Ϣ */
	PCODE_BLOCK pFirstBlock;			/* ��һ������ */
} FUNCTION, *PFUNCTION;
typedef std::vector<PFUNCTION> SYMTAB_FUNCTION_LIST, *PSYMTAB_FUNCTION_LIST;

typedef struct _GLOBAL_TYPE
{
	// STABS �� 15 λ����־λ��������(1)����������(0)�� 14 - 8 λ���ļ� Id�� 7 - 0 λ������ Id
	PSYMTAB_TYPE_NAME_LIST pNames;
	PSYMTAB_ID_LIST pIds;
	DWORD dwCurrentId;
	DWORD dwMasterId;
	BOOL bNew;

	DWORD dwFileId;						/* ������λ�ڵ��ļ� Id */
	ENUM_BASIC_TYPE BasicType;			/* �������� */
	DWORD dwTypeAliasTo;				/* ָ������ Id */
	_GLOBAL_TYPE *pTypeTo;				/* ָ�����ͣ����� Pointer �� Array �� Function �� Const */
	PSYMTAB_VARIABLE_LIST pMembers;		/* �ṹ��/������/ö�ٵĳ�Ա�������Ĳ��� */
	PSYMTAB_ARRAY_DIMS pDims;			/* ����ά�ȣ� HighPart = �Ͻ磬 LowPart = �½� */
} GLOBAL_TYPE, *PGLOBAL_TYPE;

typedef std::vector<PGLOBAL_TYPE> SYMTAB_TYPE_LIST, *PSYMTAB_TYPE_LIST;

typedef class CBaseSymtab
{
protected:
	PSYMTAB_STRING_LIST m_cFiles;
	PSYMTAB_TYPE_LIST m_cTypes;
	PSYMTAB_VARIABLE_LIST m_cVariables;
	PSYMTAB_FUNCTION_LIST m_cFunctions;

	BOOL m_b64Bit;

public:
	CBaseSymtab();
	~CBaseSymtab();

	// ͨ���ļ� Id ��ȡ�ļ�·��
	LPCTSTR GetSourceFile(DWORD dwIndex);

	// ��ȡ������
	LPCTSTR GetTypeName(DWORD dwId, DWORD dwFileId = NONE);
	LPCTSTR GetTypeName(PGLOBAL_TYPE pType, DWORD dwTypeAliasId = 0);
	// �������Ͷ���
	BOOL GetTypeDefination(DWORD dwId, _tostringstream &s, BOOL bNameOnly = FALSE, DWORD dwIndent = 0, DWORD dwFileId = NONE);
	BOOL GetTypeDefination(PGLOBAL_TYPE pType, DWORD dwId, _tostringstream &s, BOOL bNameOnly = FALSE, DWORD dwIndent = 0);
	// ���ɱ�������
	BOOL GetVariableDefination(PVARIABLE pVar, _tostringstream &s, DWORD dwIndent = 0);
	// ����һ�����͵Ĵ�С
	DWORD CalcTypeSize(PGLOBAL_TYPE pType);
	DWORD CalcTypeSize(DWORD dwTypeId, DWORD dwFileId = NONE);

	// ��ӡ���Ͷ���
	VOID PrintType(PGLOBAL_TYPE pTypeCon)
	{
		_tstring ts;
		_tostringstream s;

		if (!pTypeCon) return;

		s.str(_T(""));
		GetTypeDefination(pTypeCon, pTypeCon->dwCurrentId, s);
		ts = s.str();
		_tcout << ts;
		_tcout.flush();
	}

	VOID PrintFunction(PFUNCTION pFunc);
protected:
	BOOL DirectoryExists(LPCSTR lpPath);
	BOOL FileExists(LPCSTR lpPath);

	size_t SplitString(LPCSTR lpString, LPCSTR lpSplit, LPSTR lpResult[], size_t nCountOfResultArray);
	BOOL PathCanonicalize(LPSTR lpPath);

	size_t AddSourceFile(LPCSTR lpFileName);

	PVARIABLE CreateVariable(DWORD dwFileId = NONE);
	PVARIABLE AddVariable(PVARIABLE pVariable);
	PVARIABLE GetVariable(LPCTSTR lpName, BOOL bIgnoreCase = FALSE, DWORD dwFileId = NONE);
	PVARIABLE GetVariable(INT64 lpAddress, DWORD dwFileId = NONE);
	static VOID DestroyVariable(PVARIABLE &pVariable);	
	
	PGLOBAL_TYPE CreateGlobalType(DWORD dwFileId = NONE);
	PGLOBAL_TYPE AddGlobalType(PGLOBAL_TYPE pGlobalType);
	PGLOBAL_TYPE GetGlobalType(DWORD dwTypeId, DWORD dwFileId = NONE);
	PGLOBAL_TYPE GetGlobalType(LPCTSTR lpName, DWORD dwFileId = NONE, BOOL bIgnoreCase = FALSE);
	BOOL AddGlobalTypeAlias(PGLOBAL_TYPE pTypeCon, DWORD dwTypeId, LPTSTR lpName = NULL);
	static VOID DestroyGlobalType(PGLOBAL_TYPE &pGlobalType);

	PCODE_BLOCK CreateCodeBlock(PCODE_BLOCK pCurrentCodeBlock = NULL);
	BOOL AddCodeBlockVariable(PCODE_BLOCK pCodeBlock, PVARIABLE pVar);
	static VOID DestroyCodeBlock(PCODE_BLOCK &pCodeBlock);

	PFUNCTION CreateFunction(DWORD dwFileId = NONE);
	PFUNCTION AddFunction(PFUNCTION pFunc);
	PFUNCTION GetFunction(LPTSTR lpName, BOOL bIgnoreCase = FALSE, DWORD dwFileId = NONE);
	BOOL AddFunctionParameter(PGLOBAL_TYPE pFuncType, PVARIABLE pVar);
	PVARIABLE GetFunctionParameter(PGLOBAL_TYPE pFuncType, LPCTSTR lpName);
	BOOL AddFunctionLineNumber(PFUNCTION pFunc, DWORD dwLineNum, DWORD dwOffset);
	static VOID DestroyFunction(PFUNCTION &pFunc);

	static VOID DestroyMembers(PSYMTAB_VARIABLE_LIST &pMembers);
	static VOID DestroyArrayDims(PSYMTAB_ARRAY_DIMS &pArrayDims);
	static VOID DestroyTypeNames(PSYMTAB_TYPE_NAME_LIST &pNames);
	static VOID DestroyTypeIds(PSYMTAB_ID_LIST &pIds);
	static VOID DestroyFunctionLines(PSYMTAB_FUNCTION_LINE_LIST &pLines);

private:
	// ���ɽṹ��/�������Ա
	BOOL GetStructMember(PGLOBAL_TYPE pType, _tostringstream &s, DWORD dwIndent = 0);
	// ����ö�ٳ�Ա
	BOOL GetEnumMember(PGLOBAL_TYPE pType, _tostringstream &s, DWORD dwIndent = 0);

	// �ж�һ�������Ƿ�Ϊ��������
	BOOL IsInt(ENUM_BASIC_TYPE Type)
	{
		switch (Type)
		{
		case Byte: case Char: case Short: case Word: case Int:
		case Dword: case Longlong: case Qword:
			return TRUE;
		default:
			return FALSE;
		}
	}

	// ��ȡһ�����͵Ĵ�С (λ)
	DWORD TypeBitSize(ENUM_BASIC_TYPE Type)
	{
		switch (Type)
		{
		case Byte: case Char: return 8;
		case Short: case Word: return 16;
		case Int: case Dword: return 32;
		case Longlong: case Qword: return 64;
		default:
			return 0;
		}
	}

} CBaseSymtab, *PBaseSymtab;

inline VOID DestroyCString(LPTSTR &lpString)
{
	if (!lpString) return;

	delete [] lpString;
	lpString = NULL;
}

// �Զ����쳣����
#define SYMTAB_EXCEPTION_UNKNOWN			0	// δ֪�쳣
#define SYMTAB_EXCEPTION_FIXME				1	// δ���ǵ������
#define SYMTAB_EXCEPTION_BAD_ALLOCATION		2	// �ڴ�����ʧ��
#define SYMTAB_EXCEPTION_BAD_FORMAT			3	// ��Ч��ʽ
#define SYMTAB_EXCEPTION_UNKNOWN_TYPE		4	// ��Ч����
#define SYMTAB_EXCEPTION_UNKNOWN_RANGE		5	// ��Ч��Χ


#endif /* _BASESYMTAB_H_ */