
#ifndef _STABSYMTAB_H_
#define _STABSYMTAB_H_

#include "BaseSymtab.h"

/* 以下定义来自 bfd */

typedef enum : unsigned char
{
	/* 以下定义来自 /include/aout/adobe.h */
	N_UNDF		= 	0x00,
	N_EXT		= 	0x01,	/* 外部符号 (被期望链接到此文件) */
	N_ABS		= 	0x02,	/* 拥有绝对地址的符号 */
	N_TEXT		= 	0x04,	/* Text 段符号，偏移 */
	N_DATA		= 	0x06,	/* Data 段符号，偏移 */
	N_BSS		= 	0x08,	/* BSS  段符号，偏移 */
	N_FN_SEQ	= 	0x0C,	/* 来自于 Sequent 编译器 (sigh) 的 N_FN */
	N_COMM		= 	0x12,	/* 公共符号 (在被链接后可见) */
	N_SETA		= 	0x14,
	N_SETT		= 	0x16,
	N_SETD		= 	0x18,
	N_SETB		= 	0x1a,
	N_SETV		= 	0x1c,	/* 指向数据区集合向量的指针  */
	N_FN		= 	0x1f,	/* .o 文件名 */

	/* 以下定义来自 /include/aout/stab.def */
	N_GSYM		= 	0x20,	/* 全局变量，在多个文件中被声明 (只有名称，需从外部符号查找地址) */
	N_FNAME		= 	0x22,	/* BSD Fortran 函数名 */
	N_FUN		= 	0x24,	/* C 函数名或 text 段变量，value 指向其地址 */
	N_STSYM		= 	0x26,	/* 带有内部链接的 data 段变量，value 指向其地址 */
	N_LCSYM		= 	0x28,	/* 带有内部链接的 bss 段变量，value 指向其地址 */
	N_MAIN		= 	0x2a,	/* 主函数名称 */
	N_ROSYM		= 	0x2c,	/* 只读数据符号 */
	N_BNSYM		= 	0x2e,	/* MacOS X 可重定向函数块起始位置 */
	N_PC		= 	0x30,	/* Pascal 的全局符号 */
	N_NSYMS		= 	0x32,	/* 符号数量 */
	N_NOMAP		= 	0x34,	/* 没有 DST 符号映射 */
	N_OBJ		= 	0x38,	/* Solaris 2 .o 文件 */
	N_OPT		= 	0x3c,	/* Solaris 2 调试器选项 */
	N_RSYM		= 	0x40,	/* 寄存器变量，value 指向寄存器号 */
	N_M2C		= 	0x42,	/* Modula-2 编译单元 */
	N_SLINE		= 	0x44,	/* text 段行号，desc 为数值 */
	N_DSLINE	= 	0x46,	/* data 段行号，desc 为数值 */
	N_BSLINE	= 	0x48,	/* bss 段行号，desc 为数值 */
	N_DEFD		= 	0x4a,	/* GNU Modula-2 独立定义模块，value 为定义时间或修改时间 */
	N_FLINE		= 	0x4c,	/* Solaris2 函数起始/代码/结束行号 */
	N_ENSYM		= 	0x4e,	/* MacOS X 可重定向函数结束位置与调试信息 */
	N_EHDECL	= 	0x50,	/* GNU C++ 异常变量，name 为变量名 */
	N_CATCH		= 	0x54,	/* GNU C++ `catch' 子句，value 为其地址 */
	N_SSYM		= 	0x60,	/* 结构或联合体的成员，value 为其在此结构中的偏移 */
	N_ENDM		= 	0x62,	/* Solaris2 中此模块最后的信息 */
	N_SO		= 	0x64,	/* 主要源文件的名称 */
	N_OSO		= 	0x66,	/* Apple 中与 N_SO 关联的 .o 文件 */
	N_ALIAS		= 	0x6c,	/* SunPro F77 中的别名 */
	N_LSYM		= 	0x80,	/* 栈中的自动变量，value 为其在栈帧指针中的偏移。若偏移为 0 且不在函数中，则是全局类型 */
	N_BINCL		= 	0x82,	/* 一个包含文件的起始位置 (仅在 Sun 中使用) */
	N_SOL		= 	0x84,	/* 子源代码 (#include) 名称 */
	N_PSYM		= 	0xa0,	/* 参数变量，value 为其在参数指针 (大部分情况下就是栈帧指针) 中的偏移 */
	N_EINCL		= 	0xa2,	/* include 文件结束标志 */
	N_ENTRY		= 	0xa4,	/* 备用的入口点，value 为其地址 */
	N_LBRAC		=	0xc0,	/* 词法块 ({) 起始位置，desc 为其嵌套级别，value 为其起始地址 */
	N_EXCL		= 	0xc2,	/* 已删除的 include 文件的占位符 */
	N_SCOPE		= 	0xc4,	/* Modula-2 域信息 */
	N_PATCH		= 	0xd0,	/* Solaris2 中为运行时检查器打补丁 */
	N_RBRAC		= 	0xe0,	/* 词法块 (}) 结束位置，其 desc 匹配 N_LBRAC 的 desc，value 为其结束地址 */
	N_BCOMM		= 	0xe2,	/* 命名的公共块起始标志 */
	N_ECOMM		= 	0xe4,	/* 命名的公共块结束标志 */
	N_ECOML		= 	0xe8,	/* 公共快成员，value 为其在公共快内的偏移 */
	N_WITH		= 	0xea,	/* Solaris2 中 Pascal 的 with 语句 */
	N_NBTEXT	= 	0xf0,	/* 基于非寄存器的符号 (仅用于 Gould 系统) */
	N_NBDATA	= 	0xf2,	/* 基于非寄存器的符号 (仅用于 Gould 系统) */
	N_NBBSS		= 	0xf4,	/* 基于非寄存器的符号 (仅用于 Gould 系统) */
	N_NBSTS		= 	0xf6,	/* 基于非寄存器的符号 (仅用于 Gould 系统) */
	N_NBLCS		= 	0xf8,	/* 基于非寄存器的符号 (仅用于 Gould 系统) */
	N_LENG		=	0xfe	/* 第二符号条目入口的长度 */
} enum_stab_type; /* 符号类型定义 */

typedef unsigned int bfd_vma;

/* 以下定义来自 /include/aout/adobe.h */

#pragma pack(push)
#pragma pack(1)
typedef struct 
{
	unsigned long n_strx;		/* 字符串表中名称的索引 */
	enum_stab_type n_type;		/* 符号类型 */
	unsigned char n_other;		/* misc 信息 (通常为空) */
	unsigned short n_desc;		/* 描述字段 */
	bfd_vma n_value;			/* 符号的值 */
} internal_nlist;
#pragma pack(pop)

/* 验证一个字符串索引是否有效 */
#define VALIDATE_STRING(n) \
	if (!(n) || (n) >= m_dwStabStrSize) \
		break;

/* 在一个字符串索引无效时 */
#define VALIDATE_STRING_ELSE(n) \
	if (!(n) || (n) >= m_dwStabStrSize) \


typedef class CStabSymtab : CBaseSymtab
{
private:
	HANDLE m_hFile;
	HANDLE m_hFileMap;
	LPVOID m_pMem;

	internal_nlist *m_pStabs;
	LPSTR m_pStabStr;
	DWORD m_dwStabsCount;
	DWORD m_dwStabStrSize;

	DWORD m_dwCurrentFile;
	size_t m_nUnnamedIndex;

public:
	CStabSymtab();

	BOOL Load(LPCTSTR lpFileName);
	BOOL Load(HANDLE hFile);
	BOOL LoadFromMemory(LPVOID lpMem);

private:
	BOOL ParseDebugInformation();

	// 解析类型
	PGLOBAL_TYPE ResolveType(LPSTR &lpStabStr, DWORD dwLastId = 0, LPTSTR lpszTypeName = NULL, BOOL bResolveVoid = TRUE, BOOL bAddToList = TRUE);
	// 解析变量
	PVARIABLE ResolveVariable(LPSTR &lpStabStr, BOOL bResolveName = TRUE, BOOL bAddToList = TRUE);
	// 解析函数
	PFUNCTION ResolveFunction(LPSTR &lpStabStr);


private:
	// 查找 STABS 类型名称的结束位置
	LPSTR GetEndOfName(LPSTR lpStr);
	// 获取类型/变量/函数/参数名，并将其转换为合适的编码
	LPTSTR DumpName(LPSTR &lpName, BOOL bNoAnonymous = FALSE);
	// 生成临时类型名
	LPTSTR DumpName();
	// 获取类型的唯一标识符
	DWORD GetUniqueId(LPSTR &lpStr);
	// 根据子界类型确定基本数据类型
	ENUM_BASIC_TYPE GetBasicType(LPSTR &lpSubrange);
	// 确定浮点/复数类型
	ENUM_BASIC_TYPE GetComplexType(LPSTR &lpSubrange);

} CStabSymtab, *PStabSymtab;

#endif /* _STABSYMTAB_H_ */