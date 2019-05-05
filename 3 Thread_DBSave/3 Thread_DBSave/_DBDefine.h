#ifndef __DB_WRITER_H__
#define __DB_WRITER_H__
/*----------------------------------------------------
CREATE TABLE `account` (
`accountno` bigint(20) NOT NULL AUTO_INCREMENT,
`id` char(30) NOT NULL UNIQUE,
`password` char(70) NOT NULL,
PRIMARY KEY (`accountno`)
) ENGINE=InnoDB;
CREATE TABLE `clearstage` (
`accountno` bigint(20) NOT NULL,
`stageid` int(11) NOT NULL
) ENGINE=InnoDB;
-----------------------------------------------------*/
#define df_DB_IP		"127.0.0.1"
#define df_DB_ACCOUNT	"root"
#define df_DB_PORT		3306
#define df_DB_PASS		"1234"
#define df_DB_NAME		"game_schema"

#define df_RECONNECT_CNT 5
#define df_THREAD_CNT 2
#define df_SLEEP_MS 1

//--------------------------------------------------------------------------------------------
// DB 저장 메시지의 헤더
//--------------------------------------------------------------------------------------------
struct st_DBQUERY_HEADER
{
	WORD	Type;
	//WORD	Size;
	BYTE	Message[200];	// 모든 구조체를 커버할 수 있는 Size
	/*-------------------------------------------------------
	Q. 해당 프로젝트는 메모리 복사로 인한 성능 저하가 너무 큼
	A. 포인터를 Queue에 저장

	Q. 동적 할당 문제 발생
	A. MemoryPool 이용

	Q. 만든 MemoryPool은 현재 ObjectPool 개념이라 형식이 제한적임
	A. C 스타일로 모든 구조체를 담을 수 있는 버퍼를 마련
	--------------------------------------------------------*/
};


// 메시지 구조체 샘플

//--------------------------------------------------------------------------------------------
//  DB 저장 메시지 - 회원가입
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_NEW_ACCOUNT		0
struct st_DBQUERY_MSG_NEW_ACCOUNT
{
	char	szID[20];
	char	szPassword[20];
};

//--------------------------------------------------------------------------------------------
//  DB 저장 메시지 - 스테이지 클리어
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_STAGE_CLEAR		1
struct st_DBQUERY_MSG_STAGE_CLEAR
{
	__int64	iAccountNo;
	int	iStageID;
};

//--------------------------------------------------------------------------------------------
//  DB 저장 메시지 - 
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_PLAYER_UPDATE	2
struct st_DBQUERY_MSG_PLAYER_UPDATE
{
	__int64	iAccountNo;
	int	iLevel;
	__int64 iExp;
};

#endif