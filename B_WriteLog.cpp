//
//  B_WriteLog.cpp
//  caogangqian
//
//  Created by caogangqiang on 2019/11/25.
//  Copyright ?2019 caogangqiang. All rights reserved.
//
#include "pch.h"
#include "B_WriteLog.h"
#include <chrono> 
#include <direct.h> 
#include <io.h>
#include <sstream>


//CB_WriteLog* CB_WriteLog::Instance()
//{
//	static CB_WriteLog instance;
//	return &instance;
//}

CB_WriteLog::CB_WriteLog()
{
}


CB_WriteLog::~CB_WriteLog()
{
	//��֪ͨ�߳��˳�����
	m_bExitThread = true;
	m_bExitThreadDB = true;
	//�ȴ��߳̽���
	if (m_tLogThread.joinable())
	{
		m_tLogThread.join();
	}
    if (m_tLogThread2DB.joinable())
    {
        m_tLogThread2DB.join();
    }
	//ɾ������
	delete m_pLogQueue;
	
}

//��ʼ����־������
bool CB_WriteLog::Init(const std::string sFilePath, int iLevel, int iExpireDays, const std::string sDB, int iBufferSize)
{
	m_sFilePath = sFilePath;
    m_iSaveLevel = iLevel;
    m_iExpireDays = iExpireDays;
    m_sDB = sDB;
    m_iBufferSize = iBufferSize;
    
    //ɾ��������־������Ĭ��ģ��
    DeleteTimeOutLog();
    RegistMod("SYSLOG");

    //������־����
    try
    {
        m_pLogQueue=new LOG_DATA_QUEUE;
    }
    catch (const std::exception&)
    {
        throw "��ʼ��д��־�������:������־�������ʧ�ܡ���������������!";
    }
    
    //���ļ�
//    if (m_fFile.is_open())
//        m_fFile.close();
//    m_fFile.open(GetFileName(), std::ios::out | std::ios::app);
    
    //����д��־�߳�
    m_tLogThread = std::thread(std::mem_fn(&CB_WriteLog::TrueWriteLogThread), this);
    m_tLogThread.detach();//����
    
    m_tLogThread2DB = std::thread(std::mem_fn(&CB_WriteLog::TrueWriteLogThread), this);
    m_tLogThread.detach();//����
    
}

//ɾ��������־
void CB_WriteLog::DeleteTimeOutLog()
{
    
}

//ע��ģ�飬����ģ���Ӧ��־�ļ�
void CB_WriteLog::RegistMod(const std::string sModName)
{
	if (m_mpModInfo.find(sModName) != m_mpModInfo.end())
		return;

	std::ofstream fModFile = std::ofstream();
	GetFileName(sModName);
	m_mpModInfo.insert( std::pair<const std::string, std::ofstream>(sModName, fModFile));
}

//��ȡ��־�ļ�·��������
std::string CB_WriteLog::GetFileName(const std::string sModName)
{
	//��ȡ·��
	char   buffer[_MAX_PATH];
	getcwd(buffer, _MAX_PATH);
	
	std::string sDir(buffer);
	sDir = sDir + "\\SYSLog\\";
	
	//�ļ�
	bool b = CreateDirectory(sDir);
	
	std::string sFileName = GetCurrentSystemDay();//2006-04-10
	return sDir + sFileName+".txt";
}

//�����ļ���
bool CB_WriteLog::CreateDirectory(const std::string sFolder)
{
	std::string folder_builder;
	std::string sub;
	sub.reserve(sFolder.size());
	for (auto it = sFolder.begin(); it != sFolder.end(); ++it) {
		//cout << *(folder.end()-1) << endl;
		const char c = *it;
		sub.push_back(c);
		if (c == '\\' || it == sFolder.end() - 1) {
			folder_builder.append(sub);
			if (0 != ::_access(folder_builder.c_str(), 0)) {
				// this folder not exist
				if (0 != ::_mkdir(folder_builder.c_str())) {
					// create failed
					return false;
				}
			}
			sub.clear();
		}
	}
	return true;
}

// ��ȡϵͳ��ǰʱ��
std::string CB_WriteLog::GetCurrentSystemTime()
{
	try
	{
		auto time_now = std::chrono::system_clock::now();
		auto tt = std::chrono::system_clock::to_time_t(time_now);
		auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());
		auto duration_in_s = std::chrono::duration_cast<std::chrono::seconds>(time_now.time_since_epoch());
		auto ms = duration_in_ms - duration_in_s;
		
		struct tm* ptm = localtime(&tt);
		char date[60] = { 0 };
		sprintf(date, "%04d-%02d-%02d %02d:%02d:%02d:%03d",
			(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
			(int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec,ms);
		return std::string(date);

	}
	catch (const std::exception&)
	{
		return "";
	}
}

// ��ȡϵͳ��ǰ����
std::string CB_WriteLog::GetCurrentSystemDay()
{
	try
	{
		auto tt = std::chrono::system_clock::to_time_t
		(std::chrono::system_clock::now());
		struct tm* ptm = localtime(&tt);
		char date[60] = { 0 };
		sprintf(date, "%04d-%02d-%02d",
			(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday);
		return std::string(date);
	}
	catch (const std::exception&)
	{
		return "";
	}
}

//д��־����:����д�ַ������뻺�����
void CB_WriteLog::WriteLog(const std::string sRemark, const std::string sModName, unsigned int iLevel)
{
	if (sRemark.length() <= 0)
		return;

	std::lock_guard<std::mutex> guard(m_mWrite_mutex);//����
	//std::unique_lock<std::mutex> lock(m_mWrite_mutex);//����
	//�������ּ�����ʽ,��һ�ּ�,�ķ�С,�ڶ��ָ�����ǿ��,���ķ�Ҳ��,��������ѡ��һ��

	//�����ַ���
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].DateTime, 0, m_pLogQueue->nDTLen);
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Data,0,m_pLogQueue->nDataLen);

	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].DateTime, GetCurrentSystemTime().c_str(), m_pLogQueue->nDTLen);
 	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Data, sRemark.c_str(), sRemark.length());
	m_pLogQueue->Log_Array[m_pLogQueue->iHead].bFlag = true;
	m_pLogQueue->Log_Array[m_pLogQueue->iHead].nDataLen = sRemark.length();
	//�ƶ�����ͷ
	m_pLogQueue->iHead=(m_pLogQueue->iHead+1) % m_pLogQueue->nQueueLen;
	
}
void CB_WriteLog::WriteLog(const char* pRemark, unsigned int nLen, const std::string sModName, unsigned int iLevel)
{
	if (nullptr==pRemark || nLen<=0)
		return;
	std::lock_guard<std::mutex> guard(m_mWrite_mutex);//����
	//�����ַ���
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].DateTime, 0, m_pLogQueue->nDTLen);
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Data, 0, m_pLogQueue->nDataLen);

	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].DateTime, GetCurrentSystemTime().c_str(), m_pLogQueue->nDTLen);
	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Data, pRemark, nLen);
	m_pLogQueue->Log_Array[m_pLogQueue->iHead].bFlag = false;
	m_pLogQueue->Log_Array[m_pLogQueue->iHead].nDataLen = nLen;
	//�ƶ�����ͷ
	m_pLogQueue->iHead = (m_pLogQueue->iHead + 1) % m_pLogQueue->nQueueLen;
}

//ֹͣ�߳�
void CB_WriteLog::StopThread(unsigned int iThread)
{
	if (!iThread)
	{
		m_bExitThread = true;
		m_bExitThreadDB = true;
	}
	else if (1 == iThread)
	{
		m_bExitThread = true;
	}
	else if (2 == iThread)
	{
		m_bExitThreadDB = true;
	}
	else
		return;
}

//д��־�߳�(�ļ�)
void CB_WriteLog::TrueWriteLogThread()
{
	while(true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));//�ӳ�1����

		if (m_bExitThread)//׼���˳��߳�,�����ֳ�
		{
			return;
		}

		auto start= std::chrono::system_clock::now();
		//�µ�һ��,�½���־�ļ�
		std::string sToday = GetCurrentSystemDay();
		if (""==sToday )
			continue;

		if (m_sToday != sToday)
		{
			try
			{
				if (m_fFile.is_open())
					m_fFile.close();
				m_sToday = sToday;
				m_fFile.open(GetFileName(), std::ios::out | std::ios::app);
			}
			catch (const std::exception&)
			{
				WriteLog("������־�ļ�ʧ��!�ȴ�5�����");
				std::this_thread::sleep_for(std::chrono::seconds(5));//�ӳ�5��
				continue;
			}
		}

		//д��־
		try
		{
			if (!m_fFile.is_open())
			{
				m_fFile.open(GetFileName(), std::ios::out | std::ios::app);
			}
		}
		catch (const std::exception&)
		{
			WriteLog("����־�ļ�����!�ȴ�5�����");
			std::this_thread::sleep_for(std::chrono::seconds(5));//�ӳ�5��
			continue;
		}
		if (m_pLogQueue->iHead!=m_pLogQueue->iTail )
		{
			std::string sDateTime = m_pLogQueue->Log_Array[m_pLogQueue->iTail].DateTime;
			std::string remark=m_pLogQueue->Log_Array[m_pLogQueue->iTail].Data;
			try
			{
				std::string sTimeNow= sDateTime+" ";
				if (m_pLogQueue->Log_Array[m_pLogQueue->iTail].bFlag)
				{
					m_fFile << sTimeNow << remark << "\r\n";
				}
				else
				{
					std::string sRemark = "";
					for (int i = 0; i < m_pLogQueue->Log_Array[m_pLogQueue->iTail].nDataLen; i++)
					{
						char pChar[2];
						std::sprintf(pChar, "%02X", m_pLogQueue->Log_Array[m_pLogQueue->iTail].Data[i]);
						std::string s = pChar;
						sRemark += s;
						if (i % 2 == 1)
							sRemark += " ";
					}
					m_fFile << sTimeNow << sRemark << "\r\n";
				}
			}
			catch (const std::exception&)
			{
				WriteLog("��־д���ļ�ʧ��!�ȴ�5�����:"+remark);
				std::this_thread::sleep_for(std::chrono::seconds(5));//�ӳ�5��
				continue;
			}
			m_pLogQueue->iTail=(m_pLogQueue->iTail+1) % m_pLogQueue->nQueueLen;
		}
	}//while end
}

//д��־�߳�(���ݿ�)
void CB_WriteLog::TrueWriteLog2DBThread()
{
}
