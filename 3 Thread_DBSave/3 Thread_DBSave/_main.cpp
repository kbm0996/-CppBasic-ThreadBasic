#include <conio.h>
#include "DBWriter.h"
#include "CSystemLog.h"

bool g_bShutdown;

void ServerControl();

void main()
{
	timeBeginPeriod(1);
	_wsetlocale(LC_ALL, L"");

	LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_DEBUG);

	if (!DBStartup(df_DB_IP, df_DB_ACCOUNT, df_DB_PORT, df_DB_PASS, df_DB_NAME, df_THREAD_CNT))
		return;

	while (!g_bShutdown)
	{
		ServerControl();
		PrintDBStatus();
		Sleep(1);
	}

	DBCleanup();
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