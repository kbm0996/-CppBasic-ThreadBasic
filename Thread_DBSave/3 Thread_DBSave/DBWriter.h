#ifndef __C_DB_WRITER_H__
#define __C_DB_WRITER_H__
#pragma comment(lib,"mysql/lib/vs14/mysqlclient.lib")
#include "mysql/include/errmsg.h"
#include "mysql/include/mysql.h"
#include "CRingBuffer.h"
#include "CLFMemoryPool_TLS.h"
#include "_DBDefine.h"

/**************************************************************************
1. 외부에서 GameDB를 읽어온다던가 하는 상황은 매우 주의해야함
- 내부 외부에서 동시에 읽을때 동기화 문제 발생
캐시DB서버의 역할이지만, 없을 경우 직접 동기화를 맞춰줘야함
ex) 동기화가 필요한 쿼리 입력/처리시 카운팅(Cnt++/--)하여 0일때만 조회할 수 있게 하는 방법
2. 컨텐츠에 가까운 녀석이므로 딱히 library화할 필요 없음
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