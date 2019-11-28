//
//  B_WriteLog.h
//  caogangqian
//
//  Created by caogangqiang on 2019/11/25.
//  Copyright ?2019 caogangqiang. All rights reserved.
//

//线程缓存方式记录日志类,环境要求C++11
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

#define	   DATETIME_LEN_LOG				 24				//日期字符串长度
#define    BUFFER_SMALL_SIZE_LOG	     1024			//单条日志缓存1k
#define    BUFFER_BIG_SIZE_LOG	         5120			//队列缓存5k
#define    BUFFER_LIMIT_SIZE             256            //模块字符串长度

/*日志文件结构，SYSLOG为默认模块*/
	/*      LogFile
			├── 2019_11_24
			│   ├── mod1
			│   └── mod3
			└── 2019_11_25
				├── mod1
				└── mod2
				└── SYSLOG
	*/
//extern "C" _declspec(dllexport)

class CB_WriteLog
{
private:
	///类型声明
	//日志等级
	typedef enum _LOG_LEVEL
	{
		Log_Level_OFF = 0,
		Log_Level_FATAL,
		Log_Level_ERROR,
		Log_Level_WARN,
		Log_Level_INFO,
		Log_Level_Debug,
	}level;

	//存储待写日志的队列
	typedef struct _LOG_DATA_QUEUE
	{
		typedef struct _LOG_DATA
		{
			char			DateTime[DATETIME_LEN_LOG];					//时间:2019-11-25 16:28:47:237,最后是毫秒
			char			Data[BUFFER_SMALL_SIZE_LOG];				//数据
			char            Mod[BUFFER_LIMIT_SIZE];						//日志对应模块
			bool			bFlag;										//是否按字符串写标志
			level           iLevel;                                     //日志等级
			unsigned int	nDTLen;										//时间长度
			unsigned int	nDataLen;									//数据长度
			unsigned int	nModLen;									//模块长度
		}LOG_DATA, * PLOG_DATA;

		LOG_DATA		Log_Array[BUFFER_BIG_SIZE_LOG];					//缓存
		unsigned int	iHead;											//队列头
		unsigned int	iTail;											//队列尾
		const unsigned int	nDTLen		=	DATETIME_LEN_LOG;						//时间长度
		unsigned int		nQueueLen	=	BUFFER_BIG_SIZE_LOG;					//队列长度
		const unsigned int	nDataLen	=	BUFFER_SMALL_SIZE_LOG;					//数据长度
		const unsigned int	nModLen		=	BUFFER_LIMIT_SIZE;						//模块长度
		_LOG_DATA_QUEUE()
		{
			iHead = 0;
			iTail = 0;
		};

	}LOG_DATA_QUEUE, * PLOG_DATA_QUEUE;

	///私有变量
	unsigned int            m_iSaveLevel		= 0;					//日志记录等级
	unsigned int            m_iExpireDays		= 30;					//日志过期天数
	unsigned int            m_iBufferSize		= 1024;					//缓冲区大小
	bool                    m_bExitThread		= false;				//写日志线程退出标志（文件）
	bool                    m_bExitThreadDB		= false;				//写日志线程退出标志(数据库)
	std::string             m_sFilePath			= "";                   //日志路径
	std::string             m_sDB				= "";                   //数据库连接字符串
	std::string             m_sToday			= "";                   //记录当前日期
	std::string				m_sDelimiter		= "###";					//日志分隔符
	PLOG_DATA_QUEUE         m_pLogQueue			= nullptr;              //缓存环形队列
	std::thread				m_tLogThread		= std::thread();	    //写日志线程
	std::thread             m_tLogThread2DB		= std::thread();        //写日志线程(数据库)
	std::mutex				m_mWrite_mutex;								//日志入口函数锁

	std::map< std::string, std::shared_ptr<std::ofstream> >     m_mpModInfo;             //模块信息
	std::map< std::string, unsigned int >						m_mpSaveDate;            //过期判断
	static std::map< unsigned int, std::string >				m_mpLevelInfo;           //等级信息

	///私有函数
	void					TrueWriteLogThread();			                        //写日志到文件线程函数
	void                    TrueWriteLog2DBThread();                                //写日志到数据库线程函数
	std::string				GetFileName(const std::string sModName);			    //获取当天日志文件路径及名称
	void                    DeleteTimeOutLog(const unsigned int iExpireDays);       //删除过期日志
		//创建文件夹
	bool		CreateDirectory(const std::string sFolder);
							CB_WriteLog();											//构造函数

public:
	~CB_WriteLog();

	///公用接口
	//对象创建唯一入口
	static		CB_WriteLog*	Instance();						

	//初始化日志操作类,设置日志存放路径，记录等级，过期时间，数据库连接字符串，缓存大小, 分割符
	void        Init(const std::string sFilePath = "", const int iLevel = 0, const int iExpireDays = 30, const std::string sDB = "", const int iBufferSize = BUFFER_BIG_SIZE_LOG, const std::string sDelimiter = "###");

	//写日志接口函数-字符串
	void		WriteLog(const std::string sRemark, const unsigned int iLevel = Log_Level_Debug, const std::string sModName = "SYSLOG");

	//按十六进制写
	void		WriteLogHEX(const char* pRemark, const unsigned int nLen, const unsigned int iLevel = Log_Level_Debug, const std::string sModName = "SYSLOG");

	//停止线程(0:全部停止，1:写文件线程, 2:写数据库线程)
	void		StopThread(const unsigned int iThread);

	//注册模块
	void        RegistMod(const std::string sModName);

	///get接口
	//获取log文件位置，可以指定时间（时间格式：%d-%d-%d），模块。
	std::string		GetLogFromFile(const std::string sDate, const std::string sModName);

	//从数据库中获取log，生成对应log文件并返回文件位置, 需要提供时间区间（开始和结束），模块列表，等级列表
	std::string		GetLogFromDB(const std::string sStartDate, const std::string sEndDate, const std::vector< std::string> vModName, const std::vector<int> vLevel);

	//获取当前时间"%d-%d-%d %d:%d:%d"
	std::string		GetCurrentSystemTime();

	//获取当前日期"%d-%d-%d",iSpacing为日期间隔,默认0为当前日期
	std::string		GetSystemDay(const double iSpacing = 0);

	//获取log路径
	std::string		GetLogPath();

	//获取log分隔符
	std::string		GetLogDelimiter();

	//获取log数据库连接字符串
	std::string		GetLogDBStr();

	//获取log保存等级
	int				GetLogLevel();

	//获取log保存时间
	int				GetLogSaveDays();

	//获取log缓存大小
	int				GetLogBufferSize();
};



