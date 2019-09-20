# 멀티스레드 환경에서 DB 연동
## 📑 구성

**📋 _main** : 메인 함수, UpdateThread 콜백 함수, DBWriteThread

**📋 _DBDefine** : DB 접속 옵션, 메세지 헤더 정보

#### 📂 CBuffer : 각종 버퍼
> **📋 [CLFMemoryPool, CLFMemoryPool_TLS](https://github.com/kbm0996/-Pattern-MemoryPool)** : 락프리(Lock-free) 메모리풀 클래스, TLS 락프리 메모리풀 클래스
>
> **📋 [CRingBuffer](https://github.com/kbm0996/-DataStructure-RingBuffer)** : 링버퍼 클래스
>
> **📋 [CLFStack](https://github.com/kbm0996/-Pattern-MemoryPool)** : 락프리 스택 클래스

#### 📂 CDB : DB 관련
> **📋 [CallHttp](https://github.com/kbm0996/-SystemLink-CPPxPHPxDB)** : UTF8↔UTF16 변환 함수, Domain↔IP 변환 함수, Http GET/POST 메세지 보내기 및 받기 함수
>
> **📋 CDBConnector** : DB 연결, 쿼리 요청, 쿼리 결과 등 MySQL 클래스
>
> **📋 CDBConnector_TLS** : TLS버전 MySQL 클래스

#### 📂 CSystem : 로그, 미니덤프 관련
> 📋 [CSystemLog](https://github.com/kbm0996/-Utility-SystemLog), 📋 [APIHook, CrashDump](https://github.com/kbm0996/-Utility-Crashdump)

## 📋 _main
### ⚙ main() : 메인 함수

시스템 설정, 스레드 제어

```cpp
void main()
{
	timeBeginPeriod(1); // 정밀한 시간을 얻기 위한 설정
	_wsetlocale(LC_ALL, NULL);  // 유니코드를 출력하기 위한 설정
	LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_DEBUG); // 로그 레벨 설정
	
	// DB 접속
	pDBConnector = new mylib::CDBConnector_TLS(DB_IP,DB_ACCOUNT,DB_PASS,DB_NAME,DB_PORT);

	// 특정 스레드를 깨울 이벤트 설정
	g_hDBWriterEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hDBExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// 스레드 실행
	g_hDBWriterThread = (HANDLE)_beginthreadex(NULL, 0, DBWriterThread, 0, 0, NULL);
	for (int iCnt = 0; iCnt < df_THREAD_CNT; iCnt++)
		g_hUpdateThread[iCnt] = _beginthreadex(NULL,0,UpdateThread,(void*)iCnt,0,0);

	while (!g_bShutdown)
	{
		ServerControl(); // 서버 제어, 로그 레벨 조정

		static DWORD dwSystemTick = timeGetTime();
		DWORD dwCurrentTick = timeGetTime();
		if (dwCurrentTick - dwSystemTick >= 1000)
            		// 모니터링 등 할 일
			
		Sleep(1); // 스레드가 CPU를 독점하는 현상을 제외하기 위한 Sleep(1)
	}
    
	///////////////////////////////////////////////////////////
	// 스레드 종료 대기
	///////////////////////////////////////////////////////////
	WaitForMultipleObjects(df_THREAD_CNT, g_hUpdateThread, TRUE, INFINITE);
	for (unsigned int iCnt = 0; iCnt < df_THREAD_CNT; ++iCnt)
		CloseHandle(g_hUpdateThread[iCnt]); // 스레드 핸들 반환

	// g_hDBExitEvent가 ManualReset이어도 신호가 lost되어서 종료가 안될 수 있음
	DWORD dwExitCode = -1;
	while (dwExitCode == -1)
	{
		GetExitCodeThread(g_hDBWriterThread, &dwExitCode);
		SetEvent(g_hDBExitEvent); // 신호 재전송
	}
	WaitForSingleObject(g_hDBWriterThread, INFINITE);
	CloseHandle(g_hDBWriterThread);
	
	timeEndPeriod(1);
}
```

### ⚙ DBWriterThread() : 메세지를 처리하는 `소비자`

 외부로부터 매세지를 받고 메세지의 헤더에 따라 쿼리를 생성 및 DB에 전송하는 콜백 함수

```cpp
unsigned int __stdcall DBWriterThread(LPVOID lpParam)
{
	HANDLE hEvent[2] = { g_hDBExitEvent, g_hDBWriterEvent };
	int iRetval;
	wprintf(L"DBWriteThread Start\n");
	while (1)
	{
		// 두 이벤트 중 신호가 하나라도 오면 깨어난다
		iRetval = WaitForMultipleObjects(2, hEvent, FALSE, INFINITE);
		
		// 한 번 깨면 버퍼에 쌓여있는 모든 메세지 처리
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
					// 메세지의 페이로드를 이용해 쿼리문 작성
					st_DBQUERY_MSG_NEW_ACCOUNT *stData = (st_DBQUERY_MSG_NEW_ACCOUNT*)&pHeader->Message;
					swprintf_s(szQuery, L"INSERT INTO `account` (`id`,`password`) VALUES (\"%s\", \"%s\")", stData->szID, stData->szPassword);
					// 처리한 메세지 제거
					g_DBPacketPool.Free(pHeader);
					
					// DB에 쿼리 전송
					pDBConnector->Query(szQuery);
					break;
				}
				case df_DBQUERY_MSG_STAGE_CLEAR:
				{
					// 생략
				}
				case df_DBQUERY_MSG_PLAYER_UPDATE:
				{
					// 생략
				}
				default:
					// 생략
				}

			}
		}
		
		// 첫 번째 이벤트(g_hDBExitEvent)의 신호가 오면 스레드 종료
		if (iRetval == WAIT_OBJECT_0)
		{
			g_bShutdown = true;
			break;
		}
	}
	return 0;
}
```


### ⚙ UpdateThread() : 메세지를 전송하는 `생산자`

 다수의 클라이언트가 서버측으로 DB 관련 요청을 보낸다는 상황을 가정한 이벤트 생성 콜백 함수
 
 "\_DBDefine.h"에 명시된 3가지 메세지를 랜덤으로 생성하여 메세지 Queue에 삽입
