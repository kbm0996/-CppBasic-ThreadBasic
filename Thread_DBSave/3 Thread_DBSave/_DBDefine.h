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
// DB ���� �޽����� ���
//--------------------------------------------------------------------------------------------
struct st_DBQUERY_HEADER
{
	WORD	Type;
	//WORD	Size;
	BYTE	Message[200];	// ��� ����ü�� Ŀ���� �� �ִ� Size
	/*-------------------------------------------------------
	Q. �ش� ������Ʈ�� �޸� ����� ���� ���� ���ϰ� �ʹ� ŭ
	A. �����͸� Queue�� ����

	Q. ���� �Ҵ� ���� �߻�
	A. MemoryPool �̿�

	Q. ���� MemoryPool�� ���� ObjectPool �����̶� ������ ��������
	A. C ��Ÿ�Ϸ� ��� ����ü�� ���� �� �ִ� ���۸� ����
	--------------------------------------------------------*/
};


// �޽��� ����ü ����

//--------------------------------------------------------------------------------------------
//  DB ���� �޽��� - ȸ������
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_NEW_ACCOUNT		0
struct st_DBQUERY_MSG_NEW_ACCOUNT
{
	char	szID[20];
	char	szPassword[20];
};

//--------------------------------------------------------------------------------------------
//  DB ���� �޽��� - �������� Ŭ����
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_STAGE_CLEAR		1
struct st_DBQUERY_MSG_STAGE_CLEAR
{
	__int64	iAccountNo;
	int	iStageID;
};

//--------------------------------------------------------------------------------------------
//  DB ���� �޽��� - 
//--------------------------------------------------------------------------------------------
#define df_DBQUERY_MSG_PLAYER_UPDATE	2
struct st_DBQUERY_MSG_PLAYER_UPDATE
{
	__int64	iAccountNo;
	int	iLevel;
	__int64 iExp;
};

#endif