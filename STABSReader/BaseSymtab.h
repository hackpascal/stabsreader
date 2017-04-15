
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
	LPTSTR lpName;				/* 变量名称 */
	ENUM_VARIABLE_TYPE Type;	/* 变量类型 */
	DWORD dwTypeAliasId;		/* 变量的数据类型 Id */
	_GLOBAL_TYPE *pType;		/* 变量的数据类型 */
	WORD wLineNum;				/* 变量的行号 */
	ENUM_MEMBER_VISIBILITY Visibility;	/* 类成员可见性 */
	union
	{
		struct
		{
			DWORD dwFileId;		/* 仅用在表示变量时使用 */
			DWORD dwBitStart;	/* 仅用于结构/联合体的成员中 */
			DWORD dwBitLength;	/* 仅用于结构/联合体的成员中 */
		};
		struct
		{
			INT64 llAddress;	/* 变量地址。全局/静态变量表示绝对地址；局部变量/参数表示相对地址 */
			DWORD dwRegisterNum;/* 寄存器号。仅用于寄存器参数和寄存器变量 */
		};
	};
} VARIABLE, *PVARIABLE;
typedef std::vector<PVARIABLE> SYMTAB_VARIABLE_LIST, *PSYMTAB_VARIABLE_LIST;

typedef struct _CODE_BLOCK
{
	DWORD dwStartOffset;
	DWORD dwSize;
	PSYMTAB_VARIABLE_LIST pVars;		/* 代码块的变量 */
	std::vector<_CODE_BLOCK *> *pInnerBlocks;	/* 内层代码块 */
} CODE_BLOCK, *PCODE_BLOCK;

typedef struct _FUNCTION
{
	LPTSTR lpName;						/* 变量名称 */
	DWORD dwFileId;						/* 此类型位于的文件 Id */
	DWORD dwTypeAliasId;				/* 函数原型的类型 Id */
	_GLOBAL_TYPE *pType;				/* 函数原型的类型 */
	INT64 dwAddress;					/* 函数地址 */
	DWORD dwCodeSize;					/* 函数的代码大小 */
	
	PSYMTAB_FUNCTION_LINE_LIST pLines;	/* 函数的行号信息 */
	PCODE_BLOCK pFirstBlock;			/* 第一层代码块 */
} FUNCTION, *PFUNCTION;
typedef std::vector<PFUNCTION> SYMTAB_FUNCTION_LIST, *PSYMTAB_FUNCTION_LIST;

typedef struct _GLOBAL_TYPE
{
	// STABS 中 15 位：标志位，区别函数(1)、其他类型(0)； 14 - 8 位：文件 Id； 7 - 0 位：类型 Id
	PSYMTAB_TYPE_NAME_LIST pNames;
	PSYMTAB_ID_LIST pIds;
	DWORD dwCurrentId;
	DWORD dwMasterId;
	BOOL bNew;

	DWORD dwFileId;						/* 此类型位于的文件 Id */
	ENUM_BASIC_TYPE BasicType;			/* 基本类型 */
	DWORD dwTypeAliasTo;				/* 指向类型 Id */
	_GLOBAL_TYPE *pTypeTo;				/* 指向类型，用于 Pointer 、 Array 、 Function 和 Const */
	PSYMTAB_VARIABLE_LIST pMembers;		/* 结构体/联合体/枚举的成员、函数的参数 */
	PSYMTAB_ARRAY_DIMS pDims;			/* 数组维度， HighPart = 上界， LowPart = 下界 */
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

	// 通过文件 Id 获取文件路径
	LPCTSTR GetSourceFile(DWORD dwIndex);

	// 获取类型名
	LPCTSTR GetTypeName(DWORD dwId, DWORD dwFileId = NONE);
	LPCTSTR GetTypeName(PGLOBAL_TYPE pType, DWORD dwTypeAliasId = 0);
	// 生成类型定义
	BOOL GetTypeDefination(DWORD dwId, _tostringstream &s, BOOL bNameOnly = FALSE, DWORD dwIndent = 0, DWORD dwFileId = NONE);
	BOOL GetTypeDefination(PGLOBAL_TYPE pType, DWORD dwId, _tostringstream &s, BOOL bNameOnly = FALSE, DWORD dwIndent = 0);
	// 生成变量定义
	BOOL GetVariableDefination(PVARIABLE pVar, _tostringstream &s, DWORD dwIndent = 0);
	// 计算一个类型的大小
	DWORD CalcTypeSize(PGLOBAL_TYPE pType);
	DWORD CalcTypeSize(DWORD dwTypeId, DWORD dwFileId = NONE);

	// 打印类型定义
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
	// 生成结构体/联合体成员
	BOOL GetStructMember(PGLOBAL_TYPE pType, _tostringstream &s, DWORD dwIndent = 0);
	// 生成枚举成员
	BOOL GetEnumMember(PGLOBAL_TYPE pType, _tostringstream &s, DWORD dwIndent = 0);

	// 判断一个类型是否为整型类型
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

	// 获取一个类型的大小 (位)
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

// 自定义异常类型
#define SYMTAB_EXCEPTION_UNKNOWN			0	// 未知异常
#define SYMTAB_EXCEPTION_FIXME				1	// 未考虑到的情况
#define SYMTAB_EXCEPTION_BAD_ALLOCATION		2	// 内存申请失败
#define SYMTAB_EXCEPTION_BAD_FORMAT			3	// 无效格式
#define SYMTAB_EXCEPTION_UNKNOWN_TYPE		4	// 无效类型
#define SYMTAB_EXCEPTION_UNKNOWN_RANGE		5	// 无效范围


#endif /* _BASESYMTAB_H_ */