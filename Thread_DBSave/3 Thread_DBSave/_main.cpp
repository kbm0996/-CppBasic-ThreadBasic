#include "CDBConnector_TLS.h"
#include "CLFMemoryPool_TLS.h"
#include "_DBDefine.h"
#include "CRingBuffer.h"
#include <conio.h>
#pragma comment(lib, "Winmm.lib")

#define df_RECONNECT_CNT 5
#define df_THREAD_CNT 3
#define df_SLEEP_MS 10

/**************************************************************************
1. 외부에서 GameDB를 읽어온다던가 하는 상황은 매우 주의해야함
- 내부 외부에서 동시에 읽을때 동기화 문제 발생
캐시DB서버의 역할이지만, 없을 경우 직접 동기화를 맞춰줘야함
ex) 동기화가 필요한 쿼리 입력/처리시 카운팅(Cnt++/--)하여 0일때만 조회할 수 있게 하는 방법
2. 컨텐츠에 가까운 녀석이므로 딱히 library화할 필요 없음
**************************************************************************/

//////////////////////////////////////////////////////////////////////////
// Server
//
//////////////////////////////////////////////////////////////////////////
void ServerControl();
bool g_bShutdown;

// Event
HANDLE		g_hDBExitEvent;
HANDLE		g_hDBWriterEvent;

// Monitoring
LONG64		g_WriteTPS;
LONG		g_lJoin;
LONG		g_lStage;
LONG		g_lPlayer;

// DB
mylib::CRingBuffer g_DBQueryBuffer(20000);
mylib::CLFMemoryPool_TLS<st_DBQUERY_HEADER>	g_DBPacketPool;
mylib::CDBConnector_TLS* pDBConnector;

//////////////////////////////////////////////////////////////////////////
// Thread
//
//////////////////////////////////////////////////////////////////////////
HANDLE		g_hDBWriterThread;
HANDLE		g_hUpdateThread[df_THREAD_CNT];
unsigned int __stdcall DBWriterThread(LPVOID lpParam);
unsigned int __stdcall UpdateThread(LPVOID lpParam);

void main()
{
	timeBeginPeriod(1);
	_wsetlocale(LC_ALL, NULL);
	LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_DEBUG);

	 pDBConnector = new mylib::CDBConnector_TLS(df_DB_IP, df_DB_ACCOUNT, df_DB_PASS, df_DB_NAME, df_DB_PORT);


	g_hDBWriterEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hDBExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	g_hDBWriterThread = (HANDLE)_beginthreadex(NULL, 0, DBWriterThread, 0, 0, NULL);
	for (int iCnt = 0; iCnt < df_THREAD_CNT; iCnt++)
		g_hUpdateThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, (void*)iCnt, 0, NULL);

	while (!g_bShutdown)
	{
		ServerControl();

		static DWORD dwSystemTick = timeGetTime();
		DWORD dwCurrentTick = timeGetTime();

		if (dwCurrentTick - dwSystemTick >= 1000)
		{
			wprintf(L"===========================================\n");
			wprintf(L"DB Write TPS			:  %lld \n", g_WriteTPS);
			wprintf(L"DB Queue Size			:  %d \n", g_DBQueryBuffer.GetUseSize());
			//wprintf(L"MemPool Use			:  %d \n", g_DBPacketPool.GetAllocCount());
			//wprintf(L"MemPool Alloc			:  %d \n", g_DBPacketPool.GetBlockCount());
			wprintf(L"===========================================\n");

			dwSystemTick = dwCurrentTick;
			g_WriteTPS = 0;
		}

		Sleep(1);
	}

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

	timeEndPeriod(1);
}

void ServerControl()
{
	static bool bControlMode = false;

	//------------------------------------------
	// L : Control Lock / U : Unlock / Q : Quit
	//------------------------------------------
	//  _kbhit() 함수 자체가 느리기 때문에 사용자 혹은 더미가 많아지면 느려져서 실제 테스트시 주석처리 권장
	// 그런데도 GetAsyncKeyState를 안 쓴 이유는 창이 활성화되지 않아도 키를 인식함 Windowapi의 경우 
	// 제어가 가능하나 콘솔에선 어려움

	if (_kbhit())
	{
		WCHAR ControlKey = _getwch();

		if (L'u' == ControlKey || L'U' == ControlKey)
		{
			bControlMode = true;

			wprintf(L"[ Control Mode ] \n");
			wprintf(L"Press  L	- Key Lock \n");
			wprintf(L"Press  0~4	- Set Log Level \n");
			wprintf(L"Press  Q	- Quit \n");
		}

		if (bControlMode == true)
		{
			if (L'l' == ControlKey || L'L' == ControlKey)
			{
				wprintf(L"Controll Lock. Press U - Control Unlock \n");
				bControlMode = false;
			}

			if (L'q' == ControlKey || L'Q' == ControlKey)
			{
				g_bShutdown = true;
			}

			//------------------------------------------
			// Set Log Level
			//------------------------------------------
			if (L'0' == ControlKey)
			{
				LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_DEBUG);
				wprintf(L"Set Log - Debug, Warning, Error, System \n");

			}
			if (L'1' == ControlKey)
			{
				LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_WARNG);
				wprintf(L"Set Log - Warning, Error, System \n");
			}
			if (L'2' == ControlKey)
			{
				LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_ERROR);
				wprintf(L"Set Log - Error, System \n");
			}
			if (L'3' == ControlKey)
			{
				LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_SYSTM);
				wprintf(L"Set Log - System \n");
			}
		}
	}
}

unsigned int __stdcall DBWriterThread(LPVOID lpParam)
{
	HANDLE hEvent[2] = { g_hDBExitEvent, g_hDBWriterEvent };
	int iRetval;

	wprintf(L"DBWriteThread Start\n");
	while (1)
	{
		iRetval = WaitForMultipleObjects(2, hEvent, FALSE, INFINITE);
		while (g_DBQueryBuffer.GetUseSize() != 0)
		{
			if (g_DBQueryBuffer.GetUseSize() >= sizeof(st_DBQUERY_HEADER*))
			{
				st_DBQUERY_HEADER* pHeader;
				WCHAR szQuery[200];

				g_DBQueryBuffer.Dequeue((char*)&pHeader, sizeof(st_DBQUERY_HEADER*));
				switch (pHeader->Type)
				{
				case df_DBQUERY_MSG_NEW_ACCOUNT:
				{
					st_DBQUERY_MSG_NEW_ACCOUNT *stData = (st_DBQUERY_MSG_NEW_ACCOUNT*)&pHeader->Message;
					// TODO : 쌍따옴표로 둘러싸이지 않은 문자열은 MySql이 문자열로 인식하지 않는다.
					swprintf_s(szQuery, L"INSERT INTO `account` (`id`,`password`) VALUES (\"%s\", \"%s\")", stData->szID, stData->szPassword);
					///wprintf(L"%d %s\n", pHeader->Type, szQuery);

					g_DBPacketPool.Free(pHeader);
					pDBConnector->Query(szQuery);
					break;
				}
				case df_DBQUERY_MSG_STAGE_CLEAR:
				{
					st_DBQUERY_MSG_STAGE_CLEAR *stData = (st_DBQUERY_MSG_STAGE_CLEAR*)&pHeader->Message;
					// TODO : 숫자 데이터를 따옴표로 둘러싸면 문자열로 인식하여 MySql이 쿼리문을 실행하지 못한다.
					swprintf_s(szQuery, L"INSERT INTO `clearstage` (`accountno`,`stageid`) VALUES (%d, %d)", stData->iAccountNo, stData->iStageID);
					///wprintf(L"%d %s\n", pHeader->Type, szQuery);

					g_DBPacketPool.Free(pHeader);
					pDBConnector->Query(szQuery);
					break;
				}
				case df_DBQUERY_MSG_PLAYER_UPDATE:
				{
					st_DBQUERY_MSG_PLAYER_UPDATE *stData = (st_DBQUERY_MSG_PLAYER_UPDATE*)&pHeader->Message;
					swprintf_s(szQuery, L"INSERT INTO `player` (`accountno`,`level`,`exp`) VALUES (%d, %d, %lld)", stData->iAccountNo, stData->iLevel, stData->iExp);
					///wprintf(L"%d %s\n", pHeader->Type, szQuery);

					g_DBPacketPool.Free(pHeader);
					pDBConnector->Query(szQuery);
					break;
				}
				default:
					g_DBPacketPool.Free(pHeader);
					LOG(L"SYSTEM", LOG_SYSTM, L"DBWriteThread() Unknown Packet Header %d", pHeader->Type);
					break;
				}

			}
		}
		if (iRetval == WAIT_OBJECT_0)
		{
			g_bShutdown = true;
			break;
		}
	}
	wprintf(L"DBWriteThread End\n");
	return 0;
}

unsigned int __stdcall UpdateThread(LPVOID lpParam)
{
	srand((int)lpParam);

	st_DBQUERY_HEADER *pHeader;

	wprintf(L"%d UpdateThread Start\n", (int)lpParam);
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
			_itow_s(InterlockedIncrement(&g_lJoin), stData->szID, 10);
			_itow_s(g_lJoin, stData->szPassword, 10);
			///wprintf(L"%d %s %s\n", pHeader->Type, stData->szID, stData->szPassword);
			break;
		}
		case df_DBQUERY_MSG_STAGE_CLEAR:
		{
			pHeader->Type = df_DBQUERY_MSG_STAGE_CLEAR;
			st_DBQUERY_MSG_STAGE_CLEAR *stData = (st_DBQUERY_MSG_STAGE_CLEAR*)pHeader->Message;
			stData->iAccountNo = InterlockedIncrement(&g_lStage);
			stData->iStageID = g_lStage;
			///wprintf(L"%d %s\n", pHeader->Type, szQuery);
			break;
		}
		case df_DBQUERY_MSG_PLAYER_UPDATE:
		{
			pHeader->Type = df_DBQUERY_MSG_PLAYER_UPDATE;
			st_DBQUERY_MSG_PLAYER_UPDATE *stData = (st_DBQUERY_MSG_PLAYER_UPDATE*)&pHeader->Message;
			stData->iAccountNo = InterlockedIncrement(&g_lPlayer);
			stData->iLevel = g_lPlayer;
			stData->iExp = g_lPlayer;
			///wprintf(L"%d %s\n", pHeader->Type, szQuery);
			break;
		}
		}
		g_DBQueryBuffer.Lock();
		if (g_DBQueryBuffer.GetFreeSize() >= sizeof(st_DBQUERY_HEADER*))
			g_DBQueryBuffer.Enqueue((char*)&pHeader, sizeof(st_DBQUERY_HEADER*));
		g_DBQueryBuffer.Unlock();

		// DBWriteThread 깨우기
		SetEvent(g_hDBWriterEvent);
	}
	wprintf(L"UpdateThread End\n");
	return 0;
}