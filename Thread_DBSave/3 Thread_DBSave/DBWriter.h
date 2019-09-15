#ifndef __C_DB_WRITER_H__
#define __C_DB_WRITER_H__
#pragma comment(lib,"mysql/lib/vs14/mysqlclient.lib")
#include "mysql/include/errmsg.h"
#include "mysql/include/mysql.h"
#include "CRingBuffer.h"
#include "CLFMemoryPool_TLS.h"
#include "_DBDefine.h"

/**************************************************************************
1. �ܺο��� GameDB�� �о�´ٴ��� �ϴ� ��Ȳ�� �ſ� �����ؾ���
- ���� �ܺο��� ���ÿ� ������ ����ȭ ���� �߻�
ĳ��DB������ ����������, ���� ��� ���� ����ȭ�� ���������
ex) ����ȭ�� �ʿ��� ���� �Է�/ó���� ī����(Cnt++/--)�Ͽ� 0�϶��� ��ȸ�� �� �ְ� �ϴ� ���
2. �������� ����� �༮�̹Ƿ� ���� libraryȭ�� �ʿ� ����
**************************************************************************/

using namespace std;
using namespace mylib;
//////////////////////////////////////////////////////////////////////////
// DB Control & Log
//
//////////////////////////////////////////////////////////////////////////
bool DBStartup(char* szIP = df_DB_IP, char* szConnection = df_DB_ACCOUNT, int iPort = df_DB_PORT, char* szPass = df_DB_PASS, char* szDb = df_DB_NAME, int iUpdateThreadNo = df_THREAD_CNT);
void DBCleanup();

bool DBConnection(char* szIP, char* szConnection, int iPort, char* szPass, char* szDb);
bool DBExecQuery(char* pQuery);
void PrintDBStatus();

//////////////////////////////////////////////////////////////////////////
// Thread
//
//////////////////////////////////////////////////////////////////////////
unsigned int __stdcall DBWriterThread(LPVOID lpParam);
unsigned int __stdcall UpdateThread(LPVOID lpParam);

extern bool g_bShutdown;

#endif