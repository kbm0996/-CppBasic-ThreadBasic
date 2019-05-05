#include "DBWriter.h"
#pragma comment(lib, "Winmm.lib")

// DB
MYSQL		g_Connection;
MYSQL*		g_pConnection;
CRingBuffer g_DBRingBuffer(20000);
CLFMemoryPool_TLS<st_DBQUERY_HEADER>	g_DBPacketPool;

// Monitoring
LONG64		g_WriteTPS;
LONG		g_lJoin;
LONG		g_lStage;
LONG		g_lPlayer;

// Event
HANDLE		g_hDBExitEvent;
HANDLE		g_hDBWriterEvent;

// Thread
HANDLE		g_hDBWriterThread;
HANDLE		g_hUpdateThread[df_THREAD_CNT];

void PrintDBStatus()
{
	static DWORD dwSystemTick = timeGetTime();
	DWORD dwCurrentTick = timeGetTime();

	if (dwCurrentTick - dwSystemTick >= 1000)
	{
		printf("===========================================\n");
		printf("DB Write TPS			:  %lld \n", g_WriteTPS);
		printf("DB Queue Size			:  %d \n", g_DBRingBuffer.GetUseSize());
		//printf("MemPool Use			:  %d \n", g_DBPacketPool.GetAllocCount());
		//printf("MemPool Alloc			:  %d \n", g_DBPacketPool.GetBlockCount());
		printf("===========================================\n");

		dwSystemTick = dwCurrentTick;
		g_WriteTPS = 0;
	}
}

bool DBStartup(char* szIP, char* szConnection, int iPort, char* szPass, char* szDb, int iUpdateThreadNo)
{
	if (!DBConnection(szIP, szConnection, iPort, szPass, szDb))
		return false;

	mysql_set_character_set(g_pConnection, "utf8");

	g_hDBWriterEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hDBExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	g_hDBWriterThread = (HANDLE)_beginthreadex(NULL, 0, DBWriterThread, 0, 0, NULL);
	for (int iCnt = 0; iCnt < iUpdateThreadNo; iCnt++)
		g_hUpdateThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, (void*)iCnt, 0, NULL);

	return true;
}

void DBCleanup()
{
	WaitForMultipleObjects(df_THREAD_CNT, g_hUpdateThread, TRUE, INFINITE);
	for (unsigned int iCnt = 0; iCnt < df_THREAD_CNT; ++iCnt)
		CloseHandle(g_hUpdateThread[iCnt]);

	DWORD dwExitCode = -1;
	while (dwExitCode == -1)
	{
		GetExitCodeThread(g_hDBWriterThread, &dwExitCode);
		// g_hDBExitEvent가 ManualReset이어도 신호가 자주 Lost되어서 g_hDBWriterThread가 종료가 안되는 경우가 있음
		SetEvent(g_hDBExitEvent);
	}

	WaitForSingleObject(g_hDBWriterThread, INFINITE);
	CloseHandle(g_hDBWriterThread);

	mysql_close(g_pConnection);
}

bool DBConnection(char* szIP, char* szConnection, int iPort, char* szPass, char* szDb)
{
	int iConnectCnt = 0;

	mysql_init(&g_Connection);
	while (1)
	{
		++iConnectCnt;
		g_pConnection = mysql_real_connect(&g_Connection, szIP, szConnection, szPass, szDb, iPort, (char *)NULL, 0);
		if (g_pConnection == NULL)
		{
			fprintf(stderr, "%d Mysql connection error : %s\n", iConnectCnt, mysql_error(&g_Connection));
			if (iConnectCnt == df_RECONNECT_CNT)
				return false;
		}
		else
			break;
	}
	return true;
}

bool DBExecQuery(char * pQuery)
{
	int iError;
	int iConnectCnt = 0;
	int	iResult = mysql_query(g_pConnection, pQuery);
	if (iResult != 0)
	{
		// TODO : 연결과 관련된 에러
		//	CR_SOCKET_CREATE_ERROR
		//	CR_CONNECTION_ERROR
		//	CR_CONN_HOST_ERROR
		//	CR_SERVER_GONE_ERROR
		//	CR_SERVER_HANDSHAKE_ERR
		//	CR_SERVER_LOST
		//	CR_INVALID_CONN_HANDLE
		//
		//	위 에러들은 소켓, 연결 관련 에러로서 연결중 / 연결끊김 등의
		//	상황에 발생한다.몇몇 에러는 connect 시에 발생하는 에러도 있으나
		//	혹시 모르므로 연결관련 에러는 모두 체크 해보겠음
		//
		//	- 쿼리를 날림 : 에러발생
		//	- 연결 에러라면 재연결 시도
		//	- 연결 성공시 쿼리 날림
		//	- 연결 실패시 재연결 시도
		//	- 일정횟수 실패시 서버 종료
		iError = mysql_errno(g_pConnection);
		if (iError == CR_SOCKET_CREATE_ERROR || iError == CR_CONNECTION_ERROR ||
			iError == CR_CONN_HOST_ERROR || iError == CR_SERVER_GONE_ERROR ||
			iError == CR_SERVER_HANDSHAKE_ERR || iError == CR_SERVER_LOST ||
			iError == CR_INVALID_CONN_HANDLE)
		{
			if (!DBConnection(df_DB_IP, df_DB_ACCOUNT, df_DB_PORT, df_DB_PASS, df_DB_NAME))
			{
				SetEvent(g_hDBExitEvent);
				return false;
			}
		}
	}

	++g_WriteTPS;
	return true;
}

unsigned int __stdcall DBWriterThread(LPVOID lpParam)
{
	HANDLE hEvent[2] = { g_hDBExitEvent, g_hDBWriterEvent };
	int iRetval;

	printf("DBWriteThread Start\n");
	while (1)
	{
		iRetval = WaitForMultipleObjects(2, hEvent, FALSE, INFINITE);
		while (g_DBRingBuffer.GetUseSize() != 0)
		{
			if (g_DBRingBuffer.GetUseSize() >= sizeof(st_DBQUERY_HEADER*))
			{
				st_DBQUERY_HEADER* pHeader;
				char szQuery[200];

				g_DBRingBuffer.Dequeue((char*)&pHeader, sizeof(st_DBQUERY_HEADER*));
				switch (pHeader->Type)
				{
				case df_DBQUERY_MSG_NEW_ACCOUNT:
				{
					st_DBQUERY_MSG_NEW_ACCOUNT *stData = (st_DBQUERY_MSG_NEW_ACCOUNT*)&pHeader->Message;
					sprintf_s(szQuery, "INSERT INTO `account` (`id`,`password`) VALUES ('%s', '%s')", stData->szID, stData->szPassword);
					///printf("%d %s\n", pHeader->Type, szQuery);
					break;
				}
				case df_DBQUERY_MSG_STAGE_CLEAR:
				{
					st_DBQUERY_MSG_STAGE_CLEAR *stData = (st_DBQUERY_MSG_STAGE_CLEAR*)&pHeader->Message;
					sprintf_s(szQuery, "INSERT INTO `clearstage` (`accountno`,`stageid`) VALUES ('%lld', '%d')", stData->iAccountNo, stData->iStageID);
					///printf("%d %s\n", pHeader->Type, szQuery);
					break;
				}
				case df_DBQUERY_MSG_PLAYER_UPDATE:
				{
					st_DBQUERY_MSG_PLAYER_UPDATE *stData = (st_DBQUERY_MSG_PLAYER_UPDATE*)&pHeader->Message;
					sprintf_s(szQuery, "INSERT INTO `player` (`accountno`,`level`,`exp`) VALUES ('%lld', '%d', '%lld')", stData->iAccountNo, stData->iLevel, stData->iExp);
					///printf("%d %s\n", pHeader->Type, szQuery);
					break;
				}
				}
				g_DBPacketPool.Free(pHeader);
				DBExecQuery(szQuery);
			}
		}
		if (iRetval == WAIT_OBJECT_0)
		{
			g_bShutdown = true;
			break;
		}
	}
	printf("DBWriteThread End\n");
	return 0;
}

unsigned int __stdcall UpdateThread(LPVOID lpParam)
{
	srand((int)lpParam);

	st_DBQUERY_HEADER *pHeader;

	printf("%d UpdateThread Start\n", (int)lpParam);
	while (!g_bShutdown)
	{
		Sleep(df_SLEEP_MS);
		///g_DBPacketPool.Lock();
		pHeader = g_DBPacketPool.Alloc();
		///g_DBPacketPool.Unlock();

		switch (rand() % 3)
		{
		case df_DBQUERY_MSG_NEW_ACCOUNT:
		{
			pHeader->Type = df_DBQUERY_MSG_NEW_ACCOUNT;
			st_DBQUERY_MSG_NEW_ACCOUNT* stData = (st_DBQUERY_MSG_NEW_ACCOUNT*)pHeader->Message;
			_itoa_s(InterlockedIncrement(&g_lJoin), stData->szID, 10);
			_itoa_s(g_lJoin, stData->szPassword, 10);
			///printf("%d %s\n", pHeader->Type, szQuery);
			break;
		}
		case df_DBQUERY_MSG_STAGE_CLEAR:
		{
			pHeader->Type = df_DBQUERY_MSG_STAGE_CLEAR;
			st_DBQUERY_MSG_STAGE_CLEAR *stData = (st_DBQUERY_MSG_STAGE_CLEAR*)pHeader->Message;
			stData->iAccountNo = InterlockedIncrement(&g_lStage);
			stData->iStageID = g_lStage;
			///printf("%d %s\n", pHeader->Type, szQuery);
			break;
		}
		case df_DBQUERY_MSG_PLAYER_UPDATE:
		{
			pHeader->Type = df_DBQUERY_MSG_PLAYER_UPDATE;
			st_DBQUERY_MSG_PLAYER_UPDATE *stData = (st_DBQUERY_MSG_PLAYER_UPDATE*)&pHeader->Message;
			stData->iAccountNo = InterlockedIncrement(&g_lPlayer);
			stData->iLevel = g_lPlayer;
			stData->iExp = g_lPlayer;
			///printf("%d %s\n", pHeader->Type, szQuery);
			break;
		}
		}
		g_DBRingBuffer.Lock();
		if (g_DBRingBuffer.GetFreeSize() >= sizeof(st_DBQUERY_HEADER*))
			g_DBRingBuffer.Enqueue((char*)&pHeader, sizeof(st_DBQUERY_HEADER*));
		g_DBRingBuffer.Unlock();

		// DBWriteThread 깨우기
		SetEvent(g_hDBWriterEvent);
	}
	printf("UpdateThread End\n");
	return 0;
}