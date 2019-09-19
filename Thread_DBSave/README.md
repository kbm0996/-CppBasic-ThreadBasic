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
> **📋 [CDBConnector](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/Thread_DBSave/3%20Thread_DBSave)** : DB 연결, 쿼리 요청, 쿼리 결과 등 MySQL 클래스
>
> **📋 [CDBConnector_TLS](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/Thread_DBSave/3%20Thread_DBSave)** : TLS버전 MySQL 클래스

#### 📂 CSystem : 로그, 미니덤프 관련
> 📋 [CSystemLog](https://github.com/kbm0996/-Utility-SystemLog), 📋 [APIHook, CrashDump](https://github.com/kbm0996/-Utility-Crashdump)

## _main 
### ⚙ main() : 메인 함수

시스템 설정, 스레드 제어

```cpp
void main()
{
	timeBeginPeriod(1); // 정밀한 시간을 얻기 위한 설정
	_wsetlocale(LC_ALL, NULL);  // 유니코드를 출력하기 위한 설정
	LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_DEBUG);

	 pDBConnector = new mylib::CDBConnector_TLS(DB_IP, DB_ACCOUNT, DB_PASS, DB_NAME, DB_PORT);

    ///////////////////////////////////////////////////////////
    // 이벤트 설정
    ///////////////////////////////////////////////////////////
	g_hDBWriterEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hDBExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	g_hDBWriterThread = (HANDLE)_beginthreadex(NULL, 0, DBWriterThread, 0, 0, NULL);
	for (int iCnt = 0; iCnt < df_THREAD_CNT; iCnt++)
		g_hUpdateThread[iCnt] = _beginthreadex(NULL,0,UpdateThread,(void*)iCnt,0,0);

	while (!g_bShutdown)
	{
		ServerControl();

		static DWORD dwSystemTick = timeGetTime();
		DWORD dwCurrentTick = timeGetTime();

		if (dwCurrentTick - dwSystemTick >= 1000)
		{
            ///////////////////////////////////////////////////////////
            // 모니터링
            ///////////////////////////////////////////////////////////
			wprintf(L"===========================================\n");
			wprintf(L"DB Write TPS		:  %lld \n", g_WriteTPS);
			wprintf(L"DB Queue Size		:  %d \n", g_DBQueryBuffer.GetUseSize());
			wprintf(L"===========================================\n");

			dwSystemTick = dwCurrentTick;
			g_WriteTPS = 0;
		}

		Sleep(1);
	}
    
    ///////////////////////////////////////////////////////////
    // 스레드 종료 대기
    ///////////////////////////////////////////////////////////
	WaitForMultipleObjects(df_THREAD_CNT, g_hUpdateThread, TRUE, INFINITE);
	for (unsigned int iCnt = 0; iCnt < df_THREAD_CNT; ++iCnt)
		CloseHandle(g_hUpdateThread[iCnt]); // 스레드 핸들 반환

	DWORD dwExitCode = -1;
	while (dwExitCode == -1)
	{
		GetExitCodeThread(g_hDBWriterThread, &dwExitCode);
		// g_hDBExitEvent가 ManualReset이어도 신호가 lost되어서 종료가 안될 수 있음
		SetEvent(g_hDBExitEvent);
	}

	WaitForSingleObject(g_hDBWriterThread, INFINITE);
	CloseHandle(g_hDBWriterThread);

	timeEndPeriod(1);
}
```
