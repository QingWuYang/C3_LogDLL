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


//using namespace std;//C++B_LogStruct.h��׼�⣬�������������ռ�,����ͬ�����������ͻ
//ע���������һ��ŵ�#include�����棬��ΪһЩ����������Լ���namespace

#define	   DATETIME_LEN_LOG				 24				//�����ַ�������
#define    BUFFER_SMALL_SIZE_LOG	     1024			//����1k
#define    BUFFER_BIG_SIZE_LOG	         5120			//����5k
#define    BUFFER_LIMIT_SIZE             256            //ģ���ַ�������

//��������:�ṩ�����ӿ�,һ��д�ַ���,һ��д16��������
//CB_WriteLog* pLog = CB_WriteLog::Instance();
//pLog->WriteLog("�ַ�����־");
//char c[10] = { 0X1A,0X1B,0X1C,0X1D, 0X1F, 0X1E, 0X01, 0X11, 0X21, 0X31 };
//pLog->WriteLog(c, 10);


class _declspec(dllexport) CB_WriteLog
//�ѵ�������Ϊ��Ա�����еľ�̬��Ա�������ȿ��Դﵽ�ӳ�ʵ������Ŀ�ģ����ܴﵽ�̰߳�ȫ��Ŀ��,ͬʱ��������Զ��ͷ�.(C++11����)
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
            char            Mod[BUFFER_SMALL_SIZE_LOG];                 //��־��Ӧģ��
            bool			bFlag;										//�Ƿ��ַ���д��־
            level           iLevel;                                     //��־�ȼ�
            unsigned int	nDTLen;										//ʱ�䳤��
            unsigned int	nDataLen;									//���ݳ���
			unsigned int	nModLen;									//ģ�鳤��
        }LOG_DATA,*PLOG_DATA;
        
        LOG_DATA		Log_Array[BUFFER_BIG_SIZE_LOG];					//����
        unsigned int	iHead;											//����ͷ
        unsigned int	iTail;											//����β
        const unsigned int nDTLen = DATETIME_LEN_LOG;						//ʱ�䳤��
        const unsigned int nQueueLen = BUFFER_BIG_SIZE_LOG;					//���г���
        const unsigned int nDataLen = BUFFER_SMALL_SIZE_LOG;					//���ݳ���
		const unsigned int nModLen = BUFFER_SMALL_SIZE_LOG;					//ģ�鳤��
        _LOG_DATA_QUEUE()
        {
            iHead=0;
            iTail=0;
        };
        
    }LOG_DATA_QUEUE,*PLOG_DATA_QUEUE;
    
//    typedef struct _MOD_INFO
//    {
//        std::string             strModName;                     //ģ����
//        std::ofstream           fFile = std::ofstream();        //ģ���Ӧ�ļ�����
//        //std::mutex              Mod_mutex;                    //ģ���Ӧ��
//        //PLOG_DATA_QUEUE         pLogQueue = nullptr;            //���滷�ζ���
//    }mod;
    
    ///˽�б���
    unsigned int                                m_iSaveLevel = 0;                   //��־��¼�ȼ�
    unsigned int                                m_iExpireDays = 30;                 //��־��������
    unsigned int                                m_iBufferSize = 1024;               //��������С
    bool                                        m_bExitThread = false;              //д��־�߳��˳���־���ļ���
    bool                                        m_bExitThreadDB = false;            //д��־�߳��˳���־(���ݿ�)
    std::string                                 m_sFilePath = "";                   //��־·��
    std::string                                 m_sDB = "";                       //���ݿ������ַ���
    std::string                                 m_sToday = "";                      //��¼��ǰ����
    PLOG_DATA_QUEUE                             m_pLogQueue = nullptr;                //���滷�ζ���
    std::thread				                    m_tLogThread = std::thread();	    //д��־�߳�
    std::thread                                 m_tLogThread2DB = std::thread();        //д��־�߳�(���ݿ�)
    std::mutex				                    m_mWrite_mutex;					    //��־��ں�����
    std::map< std::string, std::shared_ptr<std::ofstream> >      m_mpModInfo;                        //ģ����Ϣ
    //std::ofstream            m_fFile = std::ofstream();            //�ļ�����
    ///˽�к���
    void					TrueWriteLogThread();			                        //д��־���ļ��̺߳���
    void                    TrueWriteLog2DBThread();                                //д��־�����ݿ��̺߳���
    std::string				GetFileName(const std::string sModName);			    //��ȡ������־�ļ�·��������
    void                    DeleteTimeOutLog();                                     //ɾ��������־
    /*      LogFile
            ������ 2019_11_24
            ��   ������ mod1
            ��   ������ mod3
            ������ 2019_11_25
                ������ mod1
                ������ mod2
				������ SYSLOG
    */
    
    //protected:
    //ע�����ﻹ��˽�г�Ա����,�����Ŀ��Ա��������,���ﲻ��.
	CB_WriteLog();											//���캯��
	
public:
	
    ~CB_WriteLog();
    
    ///���ýӿ�
    static CB_WriteLog*		Instance();						//���󴴽�Ψһ���
	void        Init(const std::string sFilePath, int iLevel, int iExpireDays, const std::string sDB, int iBufferSize);                                     //��ʼ����־������,������־���·������¼�ȼ�������ʱ�䣬���ݿ������ַ����������С
    void		WriteLog(const std::string sRemark, const std::string sModName = "SYSLOG", unsigned int iLevel = Log_Level_Debug);			    //д��־�ӿں���-�ַ���
    void		WriteLog(const char* pRemark, unsigned int nLen, const std::string sModName = "SYSLOG", unsigned int iLevel = Log_Level_Debug);				//��ʮ������д
    void		StopThread(unsigned int iThread);								//ֹͣ�߳�(0:ȫ��ֹͣ��1:д�ļ��߳�, 2:д���ݿ��߳�)
    bool		CreateDirectory(const std::string sFolder);	//�����ļ���
    void        RegistMod(const std::string sModName);        //ע��ģ��
    std::string		GetCurrentSystemTime();					//��ȡ��ǰʱ��"%d-%02d-%02d %02d:%02d:%02d"
    std::string		GetCurrentSystemDay();					//��ȡ��ǰ����"%d-%02d-%02d"
    
};



