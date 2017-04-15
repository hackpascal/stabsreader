
#ifndef _STABSYMTAB_H_
#define _STABSYMTAB_H_

#include "BaseSymtab.h"

/* ���¶������� bfd */

typedef enum : unsigned char
{
	/* ���¶������� /include/aout/adobe.h */
	N_UNDF		= 	0x00,
	N_EXT		= 	0x01,	/* �ⲿ���� (���������ӵ����ļ�) */
	N_ABS		= 	0x02,	/* ӵ�о��Ե�ַ�ķ��� */
	N_TEXT		= 	0x04,	/* Text �η��ţ�ƫ�� */
	N_DATA		= 	0x06,	/* Data �η��ţ�ƫ�� */
	N_BSS		= 	0x08,	/* BSS  �η��ţ�ƫ�� */
	N_FN_SEQ	= 	0x0C,	/* ������ Sequent ������ (sigh) �� N_FN */
	N_COMM		= 	0x12,	/* �������� (�ڱ����Ӻ�ɼ�) */
	N_SETA		= 	0x14,
	N_SETT		= 	0x16,
	N_SETD		= 	0x18,
	N_SETB		= 	0x1a,
	N_SETV		= 	0x1c,	/* ָ������������������ָ��  */
	N_FN		= 	0x1f,	/* .o �ļ��� */

	/* ���¶������� /include/aout/stab.def */
	N_GSYM		= 	0x20,	/* ȫ�ֱ������ڶ���ļ��б����� (ֻ�����ƣ�����ⲿ���Ų��ҵ�ַ) */
	N_FNAME		= 	0x22,	/* BSD Fortran ������ */
	N_FUN		= 	0x24,	/* C �������� text �α�����value ָ�����ַ */
	N_STSYM		= 	0x26,	/* �����ڲ����ӵ� data �α�����value ָ�����ַ */
	N_LCSYM		= 	0x28,	/* �����ڲ����ӵ� bss �α�����value ָ�����ַ */
	N_MAIN		= 	0x2a,	/* ���������� */
	N_ROSYM		= 	0x2c,	/* ֻ�����ݷ��� */
	N_BNSYM		= 	0x2e,	/* MacOS X ���ض���������ʼλ�� */
	N_PC		= 	0x30,	/* Pascal ��ȫ�ַ��� */
	N_NSYMS		= 	0x32,	/* �������� */
	N_NOMAP		= 	0x34,	/* û�� DST ����ӳ�� */
	N_OBJ		= 	0x38,	/* Solaris 2 .o �ļ� */
	N_OPT		= 	0x3c,	/* Solaris 2 ������ѡ�� */
	N_RSYM		= 	0x40,	/* �Ĵ���������value ָ��Ĵ����� */
	N_M2C		= 	0x42,	/* Modula-2 ���뵥Ԫ */
	N_SLINE		= 	0x44,	/* text ���кţ�desc Ϊ��ֵ */
	N_DSLINE	= 	0x46,	/* data ���кţ�desc Ϊ��ֵ */
	N_BSLINE	= 	0x48,	/* bss ���кţ�desc Ϊ��ֵ */
	N_DEFD		= 	0x4a,	/* GNU Modula-2 ��������ģ�飬value Ϊ����ʱ����޸�ʱ�� */
	N_FLINE		= 	0x4c,	/* Solaris2 ������ʼ/����/�����к� */
	N_ENSYM		= 	0x4e,	/* MacOS X ���ض���������λ���������Ϣ */
	N_EHDECL	= 	0x50,	/* GNU C++ �쳣������name Ϊ������ */
	N_CATCH		= 	0x54,	/* GNU C++ `catch' �Ӿ䣬value Ϊ���ַ */
	N_SSYM		= 	0x60,	/* �ṹ��������ĳ�Ա��value Ϊ���ڴ˽ṹ�е�ƫ�� */
	N_ENDM		= 	0x62,	/* Solaris2 �д�ģ��������Ϣ */
	N_SO		= 	0x64,	/* ��ҪԴ�ļ������� */
	N_OSO		= 	0x66,	/* Apple ���� N_SO ������ .o �ļ� */
	N_ALIAS		= 	0x6c,	/* SunPro F77 �еı��� */
	N_LSYM		= 	0x80,	/* ջ�е��Զ�������value Ϊ����ջָ֡���е�ƫ�ơ���ƫ��Ϊ 0 �Ҳ��ں����У�����ȫ������ */
	N_BINCL		= 	0x82,	/* һ�������ļ�����ʼλ�� (���� Sun ��ʹ��) */
	N_SOL		= 	0x84,	/* ��Դ���� (#include) ���� */
	N_PSYM		= 	0xa0,	/* ����������value Ϊ���ڲ���ָ�� (�󲿷�����¾���ջָ֡��) �е�ƫ�� */
	N_EINCL		= 	0xa2,	/* include �ļ�������־ */
	N_ENTRY		= 	0xa4,	/* ���õ���ڵ㣬value Ϊ���ַ */
	N_LBRAC		=	0xc0,	/* �ʷ��� ({) ��ʼλ�ã�desc Ϊ��Ƕ�׼���value Ϊ����ʼ��ַ */
	N_EXCL		= 	0xc2,	/* ��ɾ���� include �ļ���ռλ�� */
	N_SCOPE		= 	0xc4,	/* Modula-2 ����Ϣ */
	N_PATCH		= 	0xd0,	/* Solaris2 ��Ϊ����ʱ������򲹶� */
	N_RBRAC		= 	0xe0,	/* �ʷ��� (}) ����λ�ã��� desc ƥ�� N_LBRAC �� desc��value Ϊ�������ַ */
	N_BCOMM		= 	0xe2,	/* �����Ĺ�������ʼ��־ */
	N_ECOMM		= 	0xe4,	/* �����Ĺ����������־ */
	N_ECOML		= 	0xe8,	/* �������Ա��value Ϊ���ڹ������ڵ�ƫ�� */
	N_WITH		= 	0xea,	/* Solaris2 �� Pascal �� with ��� */
	N_NBTEXT	= 	0xf0,	/* ���ڷǼĴ����ķ��� (������ Gould ϵͳ) */
	N_NBDATA	= 	0xf2,	/* ���ڷǼĴ����ķ��� (������ Gould ϵͳ) */
	N_NBBSS		= 	0xf4,	/* ���ڷǼĴ����ķ��� (������ Gould ϵͳ) */
	N_NBSTS		= 	0xf6,	/* ���ڷǼĴ����ķ��� (������ Gould ϵͳ) */
	N_NBLCS		= 	0xf8,	/* ���ڷǼĴ����ķ��� (������ Gould ϵͳ) */
	N_LENG		=	0xfe	/* �ڶ�������Ŀ��ڵĳ��� */
} enum_stab_type; /* �������Ͷ��� */

typedef unsigned int bfd_vma;

/* ���¶������� /include/aout/adobe.h */

#pragma pack(push)
#pragma pack(1)
typedef struct 
{
	unsigned long n_strx;		/* �ַ����������Ƶ����� */
	enum_stab_type n_type;		/* �������� */
	unsigned char n_other;		/* misc ��Ϣ (ͨ��Ϊ��) */
	unsigned short n_desc;		/* �����ֶ� */
	bfd_vma n_value;			/* ���ŵ�ֵ */
} internal_nlist;
#pragma pack(pop)

/* ��֤һ���ַ��������Ƿ���Ч */
#define VALIDATE_STRING(n) \
	if (!(n) || (n) >= m_dwStabStrSize) \
		break;

/* ��һ���ַ���������Чʱ */
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

	// ��������
	PGLOBAL_TYPE ResolveType(LPSTR &lpStabStr, DWORD dwLastId = 0, LPTSTR lpszTypeName = NULL, BOOL bResolveVoid = TRUE, BOOL bAddToList = TRUE);
	// ��������
	PVARIABLE ResolveVariable(LPSTR &lpStabStr, BOOL bResolveName = TRUE, BOOL bAddToList = TRUE);
	// ��������
	PFUNCTION ResolveFunction(LPSTR &lpStabStr);


private:
	// ���� STABS �������ƵĽ���λ��
	LPSTR GetEndOfName(LPSTR lpStr);
	// ��ȡ����/����/����/��������������ת��Ϊ���ʵı���
	LPTSTR DumpName(LPSTR &lpName, BOOL bNoAnonymous = FALSE);
	// ������ʱ������
	LPTSTR DumpName();
	// ��ȡ���͵�Ψһ��ʶ��
	DWORD GetUniqueId(LPSTR &lpStr);
	// �����ӽ�����ȷ��������������
	ENUM_BASIC_TYPE GetBasicType(LPSTR &lpSubrange);
	// ȷ������/��������
	ENUM_BASIC_TYPE GetComplexType(LPSTR &lpSubrange);

} CStabSymtab, *PStabSymtab;

#endif /* _STABSYMTAB_H_ */