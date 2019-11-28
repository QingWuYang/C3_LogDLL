//
//  B_WriteLog.h
//  caogangqian
//
//  Created by caogangqiang on 2019/11/25.
//  Copyright ?2019 caogangqiang. All rights reserved.
//

//�̻߳��淽ʽ��¼��־��,����Ҫ��C++11
#include <string>
#include <fstream>
#include <thread>
#include <mutex>
#include <map>
#include <functional> 
#include <chrono> 
#include <direct.h> 
#include <io.h>
#include <sstream>
#include <memory>
#include <iostream>
#include <vector>

#define	   DATETIME_LEN_LOG				 24				//�����ַ�������
#define    BUFFER_SMALL_SIZE_LOG	     1024			//������־����1k
#define    BUFFER_BIG_SIZE_LOG	         5120			//���л���5k
#define    BUFFER_LIMIT_SIZE             256            //ģ���ַ�������

/*��־�ļ��ṹ��SYSLOGΪĬ��ģ��*/
	/*      LogFile
			������ 2019_11_24
			��   ������ mod1
			��   ������ mod3
			������ 2019_11_25
				������ mod1
				������ mod2
				������ SYSLOG
	*/
//extern "C" _declspec(dllexport)

class CB_WriteLog
{
private:
	///��������
	//��־�ȼ�
	typedef enum _LOG_LEVEL
	{
		Log_Level_OFF = 0,
		Log_Level_FATAL,
		Log_Level_ERROR,
		Log_Level_WARN,
		Log_Level_INFO,
		Log_Level_Debug,
	}level;

	//�洢��д��־�Ķ���
	typedef struct _LOG_DATA_QUEUE
	{
		typedef struct _LOG_DATA
		{
			char			DateTime[DATETIME_LEN_LOG];					//ʱ��:2019-11-25 16:28:47:237,����Ǻ���
			char			Data[BUFFER_SMALL_SIZE_LOG];				//����
			char            Mod[BUFFER_LIMIT_SIZE];						//��־��Ӧģ��
			bool			bFlag;										//�Ƿ��ַ���д��־
			level           iLevel;                                     //��־�ȼ�
			unsigned int	nDTLen;										//ʱ�䳤��
			unsigned int	nDataLen;									//���ݳ���
			unsigned int	nModLen;									//ģ�鳤��
		}LOG_DATA, * PLOG_DATA;

		LOG_DATA		Log_Array[BUFFER_BIG_SIZE_LOG];					//����
		unsigned int	iHead;											//����ͷ
		unsigned int	iTail;											//����β
		const unsigned int	nDTLen		=	DATETIME_LEN_LOG;						//ʱ�䳤��
		unsigned int		nQueueLen	=	BUFFER_BIG_SIZE_LOG;					//���г���
		const unsigned int	nDataLen	=	BUFFER_SMALL_SIZE_LOG;					//���ݳ���
		const unsigned int	nModLen		=	BUFFER_LIMIT_SIZE;						//ģ�鳤��
		_LOG_DATA_QUEUE()
		{
			iHead = 0;
			iTail = 0;
		};

	}LOG_DATA_QUEUE, * PLOG_DATA_QUEUE;

	///˽�б���
	unsigned int            m_iSaveLevel		= 0;					//��־��¼�ȼ�
	unsigned int            m_iExpireDays		= 30;					//��־��������
	unsigned int            m_iBufferSize		= 1024;					//��������С
	bool                    m_bExitThread		= false;				//д��־�߳��˳���־���ļ���
	bool                    m_bExitThreadDB		= false;				//д��־�߳��˳���־(���ݿ�)
	std::string             m_sFilePath			= "";                   //��־·��
	std::string             m_sDB				= "";                   //���ݿ������ַ���
	std::string             m_sToday			= "";                   //��¼��ǰ����
	std::string				m_sDelimiter		= "###";					//��־�ָ���
	PLOG_DATA_QUEUE         m_pLogQueue			= nullptr;              //���滷�ζ���
	std::thread				m_tLogThread		= std::thread();	    //д��־�߳�
	std::thread             m_tLogThread2DB		= std::thread();        //д��־�߳�(���ݿ�)
	std::mutex				m_mWrite_mutex;								//��־��ں�����

	std::map< std::string, std::shared_ptr<std::ofstream> >     m_mpModInfo;             //ģ����Ϣ
	std::map< std::string, unsigned int >						m_mpSaveDate;            //�����ж�
	static std::map< unsigned int, std::string >				m_mpLevelInfo;           //�ȼ���Ϣ

	///˽�к���
	void					TrueWriteLogThread();			                        //д��־���ļ��̺߳���
	void                    TrueWriteLog2DBThread();                                //д��־�����ݿ��̺߳���
	std::string				GetFileName(const std::string sModName);			    //��ȡ������־�ļ�·��������
	void                    DeleteTimeOutLog(const unsigned int iExpireDays);       //ɾ��������־
		//�����ļ���
	bool		CreateDirectory(const std::string sFolder);
							CB_WriteLog();											//���캯��

public:
	~CB_WriteLog();

	///���ýӿ�
	//���󴴽�Ψһ���
	static		CB_WriteLog*	Instance();						

	//��ʼ����־������,������־���·������¼�ȼ�������ʱ�䣬���ݿ������ַ����������С, �ָ��
	void        Init(const std::string sFilePath = "", const int iLevel = 0, const int iExpireDays = 30, const std::string sDB = "", const int iBufferSize = BUFFER_BIG_SIZE_LOG, const std::string sDelimiter = "###");

	//д��־�ӿں���-�ַ���
	void		WriteLog(const std::string sRemark, const unsigned int iLevel = Log_Level_Debug, const std::string sModName = "SYSLOG");

	//��ʮ������д
	void		WriteLogHEX(const char* pRemark, const unsigned int nLen, const unsigned int iLevel = Log_Level_Debug, const std::string sModName = "SYSLOG");

	//ֹͣ�߳�(0:ȫ��ֹͣ��1:д�ļ��߳�, 2:д���ݿ��߳�)
	void		StopThread(const unsigned int iThread);

	//ע��ģ��
	void        RegistMod(const std::string sModName);

	///get�ӿ�
	//��ȡlog�ļ�λ�ã�����ָ��ʱ�䣨ʱ���ʽ��%d-%d-%d����ģ�顣
	std::string		GetLogFromFile(const std::string sDate, const std::string sModName);

	//�����ݿ��л�ȡlog�����ɶ�Ӧlog�ļ��������ļ�λ��, ��Ҫ�ṩʱ�����䣨��ʼ�ͽ�������ģ���б��ȼ��б�
	std::string		GetLogFromDB(const std::string sStartDate, const std::string sEndDate, const std::vector< std::string> vModName, const std::vector<int> vLevel);

	//��ȡ��ǰʱ��"%d-%d-%d %d:%d:%d"
	std::string		GetCurrentSystemTime();

	//��ȡ��ǰ����"%d-%d-%d",iSpacingΪ���ڼ��,Ĭ��0Ϊ��ǰ����
	std::string		GetSystemDay(const double iSpacing = 0);

	//��ȡlog·��
	std::string		GetLogPath();

	//��ȡlog�ָ���
	std::string		GetLogDelimiter();

	//��ȡlog���ݿ������ַ���
	std::string		GetLogDBStr();

	//��ȡlog����ȼ�
	int				GetLogLevel();

	//��ȡlog����ʱ��
	int				GetLogSaveDays();

	//��ȡlog�����С
	int				GetLogBufferSize();
};



