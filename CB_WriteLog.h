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


//using namespace std;//C++B_LogStruct.h标准库，尽量不打开整个空间,避免同其他基础库冲突
//注意这条语句一般放到#include语句后面，因为一些库里会声明自己的namespace

#define	   DATETIME_LEN_LOG				 24				//日期字符串长度
#define    BUFFER_SMALL_SIZE_LOG	     1024			//缓存1k
#define    BUFFER_BIG_SIZE_LOG	         5120			//缓存5k
#define    BUFFER_LIMIT_SIZE             256            //模块字符串长度

//调用例程:提供两个接口,一种写字符串,一种写16进制数据
//CB_WriteLog* pLog = CB_WriteLog::Instance();
//pLog->WriteLog("字符串日志");
//char c[10] = { 0X1A,0X1B,0X1C,0X1D, 0X1F, 0X1E, 0X01, 0X11, 0X21, 0X31 };
//pLog->WriteLog(c, 10);


class _declspec(dllexport) CB_WriteLog
//把单件声明为成员函数中的静态成员，这样既可以达到延迟实例化的目的，又能达到线程安全的目的,同时程序结束自动释放.(C++11保障)
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
            char            Mod[BUFFER_SMALL_SIZE_LOG];                 //日志对应模块
            bool			bFlag;										//是否按字符串写标志
            level           iLevel;                                     //日志等级
            unsigned int	nDTLen;										//时间长度
            unsigned int	nDataLen;									//数据长度
			unsigned int	nModLen;									//模块长度
        }LOG_DATA,*PLOG_DATA;
        
        LOG_DATA		Log_Array[BUFFER_BIG_SIZE_LOG];					//缓存
        unsigned int	iHead;											//队列头
        unsigned int	iTail;											//队列尾
        const unsigned int nDTLen = DATETIME_LEN_LOG;						//时间长度
        const unsigned int nQueueLen = BUFFER_BIG_SIZE_LOG;					//队列长度
        const unsigned int nDataLen = BUFFER_SMALL_SIZE_LOG;					//数据长度
		const unsigned int nModLen = BUFFER_SMALL_SIZE_LOG;					//模块长度
        _LOG_DATA_QUEUE()
        {
            iHead=0;
            iTail=0;
        };
        
    }LOG_DATA_QUEUE,*PLOG_DATA_QUEUE;
    
//    typedef struct _MOD_INFO
//    {
//        std::string             strModName;                     //模块名
//        std::ofstream           fFile = std::ofstream();        //模块对应文件对象
//        //std::mutex              Mod_mutex;                    //模块对应锁
//        //PLOG_DATA_QUEUE         pLogQueue = nullptr;            //缓存环形队列
//    }mod;
    
    ///私有变量
    unsigned int                                m_iSaveLevel = 0;                   //日志记录等级
    unsigned int                                m_iExpireDays = 30;                 //日志过期天数
    unsigned int                                m_iBufferSize = 1024;               //缓冲区大小
    bool                                        m_bExitThread = false;              //写日志线程退出标志（文件）
    bool                                        m_bExitThreadDB = false;            //写日志线程退出标志(数据库)
    std::string                                 m_sFilePath = "";                   //日志路径
    std::string                                 m_sDB = "";                       //数据库连接字符串
    std::string                                 m_sToday = "";                      //记录当前日期
    PLOG_DATA_QUEUE                             m_pLogQueue = nullptr;                //缓存环形队列
    std::thread				                    m_tLogThread = std::thread();	    //写日志线程
    std::thread                                 m_tLogThread2DB = std::thread();        //写日志线程(数据库)
    std::mutex				                    m_mWrite_mutex;					    //日志入口函数锁
    std::map< std::string, std::shared_ptr<std::ofstream> >      m_mpModInfo;                        //模块信息
    //std::ofstream            m_fFile = std::ofstream();            //文件对象
    ///私有函数
    void					TrueWriteLogThread();			                        //写日志到文件线程函数
    void                    TrueWriteLog2DBThread();                                //写日志到数据库线程函数
    std::string				GetFileName(const std::string sModName);			    //获取当天日志文件路径及名称
    void                    DeleteTimeOutLog();                                     //删除过期日志
    /*      LogFile
            ├── 2019_11_24
            │   ├── mod1
            │   └── mod3
            └── 2019_11_25
                ├── mod1
                └── mod2
				└── SYSLOG
    */
    
    //protected:
    //注意这里还是私有成员变量,保护的可以被子类调用,这里不用.
	CB_WriteLog();											//构造函数
	
public:
	
    ~CB_WriteLog();
    
    ///公用接口
    static CB_WriteLog*		Instance();						//对象创建唯一入口
	void        Init(const std::string sFilePath, int iLevel, int iExpireDays, const std::string sDB, int iBufferSize);                                     //初始化日志操作类,设置日志存放路径，记录等级，过期时间，数据库连接字符串，缓存大小
    void		WriteLog(const std::string sRemark, const std::string sModName = "SYSLOG", unsigned int iLevel = Log_Level_Debug);			    //写日志接口函数-字符串
    void		WriteLog(const char* pRemark, unsigned int nLen, const std::string sModName = "SYSLOG", unsigned int iLevel = Log_Level_Debug);				//按十六进制写
    void		StopThread(unsigned int iThread);								//停止线程(0:全部停止，1:写文件线程, 2:写数据库线程)
    bool		CreateDirectory(const std::string sFolder);	//创建文件夹
    void        RegistMod(const std::string sModName);        //注册模块
    std::string		GetCurrentSystemTime();					//获取当前时间"%d-%02d-%02d %02d:%02d:%02d"
    std::string		GetCurrentSystemDay();					//获取当前日期"%d-%02d-%02d"
    
};



