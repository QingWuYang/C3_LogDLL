//
//  CB_WriteLog.cpp
//  caogangqian
//
//  Created by caogangqiang on 2019/11/25.
//  Copyright ?2019 caogangqiang. All rights reserved.
//
#include "pch.h"
#include "CB_WriteLog.h"


std::map< unsigned int, std::string > CB_WriteLog::m_mpLevelInfo = {
		{0,"OFF"},
		{1,"FATAL"},
		{2,"ERROR"},
		{3,"WARN"},
		{4,"INFO"},
		{5,"Debug"},
};

//��ȡʵ������
/*extern "C" _declspec(dllexport)*/ CB_WriteLog* CB_WriteLog::Instance()
{
	std::cout << "instance log operator!!" << std::endl;
	static CB_WriteLog instance;
	return &instance;
}

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

	for (auto item = m_mpModInfo.begin(); item != m_mpModInfo.end(); item++)
	{
		item->second->close();
	}

}

// ��ʼ����־������
void CB_WriteLog::Init(const std::string sFilePath, const int iLevel, const int iExpireDays, const std::string sDB, const int iBufferSize, const std::string sDelimiter)
{
	std::cout << "init log operator!!" << std::endl;
	m_sFilePath = sFilePath;
	m_iSaveLevel = iLevel;
	m_iExpireDays = iExpireDays;
	m_sDB = sDB;
	m_iBufferSize = iBufferSize;
	m_sToday = GetSystemDay();
	m_sDelimiter = sDelimiter;

	// Ӧ�������������
	for (int iCount = 0; iCount < m_iExpireDays; ++iCount)
	{
		m_mpSaveDate.insert(std::pair<std::string, unsigned int>(GetSystemDay(-iCount), 1));
	}

	// �������Ŀ¼
	bool b = CreateDirectory(m_sFilePath);

	// ɾ��������־������Ĭ��ģ��SYSLOG
	DeleteTimeOutLog(m_iExpireDays);
	RegistMod("SYSLOG");

	// ������־����
	try
	{
		m_pLogQueue = new LOG_DATA_QUEUE;
		m_pLogQueue->nQueueLen = iBufferSize;
	}
	catch (const std::exception&)
	{
		throw "��ʼ��д��־�������:������־�������ʧ�ܡ���������������!";
	}

	// �������ݿ�


	// ����д��־�߳�
	m_tLogThread = std::thread(std::mem_fn(&CB_WriteLog::TrueWriteLogThread), this);
	m_tLogThread.detach();//����

	m_tLogThread2DB = std::thread(std::mem_fn(&CB_WriteLog::TrueWriteLog2DBThread), this);
	m_tLogThread2DB.detach();//����

}

// ��ȡĳһ�ļ����������ļ�
void GetListFiles(std::string path, std::vector<std::string>& files) {
	// �ļ����
	long hFile = 0;
	// �ļ���Ϣ
	struct _finddata_t fileinfo;
	std::string p;
	if ((hFile = _findfirst(p.assign(path).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do {
			//�Ƚ��ļ������Ƿ����ļ���
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
				{
					files.push_back(/*p.assign(path).append("\\").append(*/fileinfo.name);
				}
			}
			else {
				files.push_back(/*p.assign(path).append("\\").append(*/fileinfo.name);
			}
		} while (_findnext(hFile, &fileinfo) == 0);  // Ѱ����һ�����ɹ�����0������-1
		_findclose(hFile);
	}
}

// �ж��Ƿ���".."Ŀ¼��"."Ŀ¼
bool IsSpecialDir(const char* path)
{
	return strcmp(path, "..") == 0 || strcmp(path, ".") == 0;
}

// �ж��ļ�������Ŀ¼�����ļ�
bool IsDir(int attrib)
{
	return attrib == 16 || attrib == 18 || attrib == 20;
}

// ��ʾɾ��ʧ��ԭ��
void ShowError(const char* file_name = NULL)
{
	errno_t err;
	_get_errno(&err);
	switch (err)
	{
	case ENOTEMPTY:
		printf("Given path is not a directory, the directory is not empty, or the directory is either the current working directory or the root directory.\n");
		break;
	case ENOENT:
		printf("Path is invalid.\n");
		break;
	case EACCES:
		printf("%s had been opend by some application, can't delete.\n", file_name);
		break;
	}
}

void GetFilePath(const char* path, const char* file_name, char* file_path)
{
	strcpy_s(file_path, sizeof(char) * _MAX_PATH, path);
	file_path[strlen(file_path) - 1] = '\0';
	strcat_s(file_path, sizeof(char) * _MAX_PATH, file_name);
}

// �ݹ�����Ŀ¼���ļ���ɾ��
void DeleteFile(const char* path)
{
	char pcSearchPath[_MAX_PATH];
	sprintf_s(pcSearchPath, _MAX_PATH, "%s\\*", path); //pcSearchPath Ϊ����·����* ����ͨ���

	// �ļ�����Ϣ
	_finddata_t DirInfo;
	// �ļ���Ϣ
	_finddata_t FileInfo;
	// ���Ҿ��
	intptr_t f_handle;

	char pcTempPath[_MAX_PATH];
	if ((f_handle = _findfirst(pcSearchPath, &DirInfo)) != -1)
	{
		while (_findnext(f_handle, &FileInfo) == 0)
		{
			if (IsSpecialDir(FileInfo.name))
				continue;
			// �����Ŀ¼������������·��
			if (FileInfo.attrib & _A_SUBDIR)
			{
				GetFilePath(pcSearchPath, FileInfo.name, pcTempPath);
				// ��ʼ�ݹ�ɾ��Ŀ¼�е�����
				DeleteFile(pcTempPath);
				if (FileInfo.attrib == 20)
					printf("This is system file, can't delete!\n");
				else
				{
					// ɾ����Ŀ¼�������ڵݹ鷵��ǰ����_findclose,�����޷�ɾ��Ŀ¼
					if (_rmdir(pcTempPath) == -1)
					{
						// Ŀ¼�ǿ������ʾ����ԭ��
						ShowError();
					}
				}
			}
			else
			{
				strcpy_s(pcTempPath, pcSearchPath);
				pcTempPath[strlen(pcTempPath) - 1] = '\0';
				// �����������ļ�·��
				strcat_s(pcTempPath, FileInfo.name);

				if (remove(pcTempPath) == -1)
				{
					ShowError(FileInfo.name);
				}

			}
		}
		// �رմ򿪵��ļ���������ͷŹ�����Դ�������޷�ɾ����Ŀ¼
		_findclose(f_handle);
	}
	else
	{
		// ��·�������ڣ���ʾ������Ϣ
		ShowError();
	}
}

void DeleteAllFile(const char* pcPath)
{
	DeleteFile(pcPath); //ɾ�����ļ�����������ļ�

	if (_rmdir(pcPath) == -1) //ɾ�����ļ���
	{
		ShowError();
	}
}

// ɾ��������־
void CB_WriteLog::DeleteTimeOutLog(const unsigned int iExpireDays)
{
	// ����
	std::vector < std::string > vListLogFile;
	GetListFiles(m_sFilePath, vListLogFile);
	for (auto iter = vListLogFile.cbegin(); iter != vListLogFile.cend(); ++iter)
	{
		if (m_mpSaveDate.find(*iter) == m_mpSaveDate.end())
		{
			// ɾ��������־
			std::string sDeleteFilePath = m_sFilePath + "\\" + *iter;
			DeleteAllFile(sDeleteFilePath.c_str());
		}
	}
}

// ע��ģ�飬����ģ���Ӧ��־�ļ�
void CB_WriteLog::RegistMod(const std::string sModName)
{
	if ("" == sModName)
		return;
	std::lock_guard<std::mutex> guard(m_mWrite_mutex);//����

	if (m_mpModInfo.find(sModName) != m_mpModInfo.end())
		return;
	std::string sModFileName = GetFileName(sModName);
	m_mpModInfo.insert(std::pair<const std::string, std::shared_ptr<std::ofstream>>(sModName, std::make_shared<std::ofstream>(sModFileName, std::ios::out | std::ios::app)));
}

// ��ȡ��־�ļ�·��������
std::string CB_WriteLog::GetFileName(const std::string sModName)
{
	//��ȡ·��
	char   buffer[_MAX_PATH];
	if ("" == m_sFilePath)
	{
		_getcwd(buffer, _MAX_PATH);
	}
	else
	{
		std::memcpy(buffer, m_sFilePath.c_str(), m_sFilePath.length());
		buffer[m_sFilePath.length()] = '\0';
	}
	std::string sFileName = GetSystemDay();//2006-04-10

	std::string sDir(buffer);
	sDir = sDir + "/" + sFileName + "/";

	//ʱ��Ŀ¼
	bool b = CreateDirectory(sDir);

	return sDir + sModName + ".txt";
}

// �����ļ���
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
			(int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec, ms);
		return std::string(date);
	}
	catch (const std::exception&)
	{
		return "";
	}
}

// ��ȡϵͳ��ǰ����
std::string CB_WriteLog::GetSystemDay(const double iSpacing)
{
	try
	{
		auto tt = std::chrono::system_clock::to_time_t
		(std::chrono::system_clock::now());
		if(iSpacing)
		{
			tt += iSpacing * 60 * 60 * 24 + 28800;
		}
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
void CB_WriteLog::WriteLog(const std::string sRemark, const unsigned int iLevel, const std::string sModName)
{
	if (sRemark.length() <= 0)
		return;
	//����¼���õȼ����ϵ���־
	if (iLevel > m_iSaveLevel)
		return;

	std::lock_guard<std::mutex> guard(m_mWrite_mutex);//����
	//std::unique_lock<std::mutex> lock(m_mWrite_mutex);//����
	//�������ּ�����ʽ,��һ�ּ�,�ķ�С,�ڶ��ָ�����ǿ��,���ķ�Ҳ��,��������ѡ��һ��

	//�����ַ���
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].DateTime, 0, m_pLogQueue->nDTLen);
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Data, 0, m_pLogQueue->nDataLen);
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Mod, 0, m_pLogQueue->nModLen);

	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].DateTime, GetCurrentSystemTime().c_str(), m_pLogQueue->nDTLen);
	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Data, sRemark.c_str(), sRemark.length());
	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Mod, sModName.c_str(), sModName.length());

	m_pLogQueue->Log_Array[m_pLogQueue->iHead].iLevel = (level)iLevel;
	m_pLogQueue->Log_Array[m_pLogQueue->iHead].bFlag = true;
	m_pLogQueue->Log_Array[m_pLogQueue->iHead].nDataLen = sRemark.length();

	// �ƶ�����ͷ
	m_pLogQueue->iHead = (m_pLogQueue->iHead + 1) % m_pLogQueue->nQueueLen;

}

//��16���Ʒ�ʽд��־
void CB_WriteLog::WriteLogHEX(const char* pRemark, const unsigned int nLen, const unsigned int iLevel, const std::string sModName)
{
	if (nullptr == pRemark || nLen <= 0)
		return;

	// ����
	std::lock_guard<std::mutex> guard(m_mWrite_mutex);

	// �����ַ���
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].DateTime, 0, m_pLogQueue->nDTLen);
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Data, 0, m_pLogQueue->nDataLen);
	std::memset(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Mod, 0, m_pLogQueue->nModLen);

	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].DateTime, GetCurrentSystemTime().c_str(), m_pLogQueue->nDTLen);
	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Data, pRemark, nLen);
	std::memcpy(m_pLogQueue->Log_Array[m_pLogQueue->iHead].Mod, sModName.c_str(), sModName.length());

	m_pLogQueue->Log_Array[m_pLogQueue->iHead].iLevel = (level)iLevel;
	m_pLogQueue->Log_Array[m_pLogQueue->iHead].bFlag = false;
	m_pLogQueue->Log_Array[m_pLogQueue->iHead].nDataLen = nLen;

	// �ƶ�����ͷ
	m_pLogQueue->iHead = (m_pLogQueue->iHead + 1) % m_pLogQueue->nQueueLen;
}

// ֹͣ�߳�
void CB_WriteLog::StopThread(const unsigned int iThread)
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

// д��־�߳�(�ļ�)
void CB_WriteLog::TrueWriteLogThread()
{
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));//�ӳ�1����
		if (m_bExitThread)//׼���˳��߳�,�����ֳ�
		{
			return;
		}

		// �µ�һ��,�½���־�ļ�
		auto start = std::chrono::system_clock::now();
		std::string sToday = GetSystemDay();
		if ("" == sToday)
			continue;
		if (m_sToday != sToday)
		{
			try
			{
				for (auto item = m_mpModInfo.begin(); item != m_mpModInfo.end(); item++)
				{
					if (item->second->is_open())
						item->second->close();
					item->second->open(GetFileName(item->first), std::ios::out | std::ios::app);
				}
				m_sToday = sToday;
			}
			catch (const std::exception&)
			{
				WriteLog("������־�ļ�ʧ��!�ȴ�5�����");
				std::this_thread::sleep_for(std::chrono::seconds(5));//�ӳ�5��
				continue;
			}
		}
		
		// д��־
		try
		{
			if (m_mpModInfo.find(m_pLogQueue->Log_Array[m_pLogQueue->iTail].Mod) == m_mpModInfo.end())
				RegistMod(m_pLogQueue->Log_Array[m_pLogQueue->iTail].Mod);
			if (!m_mpModInfo.at(m_pLogQueue->Log_Array[m_pLogQueue->iTail].Mod)->is_open())
				m_mpModInfo.at(m_pLogQueue->Log_Array[m_pLogQueue->iTail].Mod)->open(GetFileName(m_pLogQueue->Log_Array[m_pLogQueue->iTail].Mod), std::ios::out | std::ios::app);
		}
		catch (const std::exception&)
		{
			WriteLog("����־�ļ�����!�ȴ�5�����");
			std::this_thread::sleep_for(std::chrono::seconds(5));//�ӳ�5��
			continue;
		}
		if (m_pLogQueue->iHead != m_pLogQueue->iTail)
		{
			std::string sDateTime = m_pLogQueue->Log_Array[m_pLogQueue->iTail].DateTime;
			std::string remark = m_pLogQueue->Log_Array[m_pLogQueue->iTail].Data;
			std::string sLogLevel = m_mpLevelInfo[m_pLogQueue->Log_Array[m_pLogQueue->iTail].iLevel] + " ";
			try
			{
				std::string sTimeNow = sDateTime + " ";
				if (m_pLogQueue->Log_Array[m_pLogQueue->iTail].bFlag)
				{
					*m_mpModInfo[m_pLogQueue->Log_Array[m_pLogQueue->iTail].Mod] << sTimeNow << m_sDelimiter << sLogLevel << m_sDelimiter << remark << "\r\n";
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
					*m_mpModInfo[m_pLogQueue->Log_Array[m_pLogQueue->iTail].Mod] << sTimeNow << m_sDelimiter << sLogLevel << m_sDelimiter << sRemark << "\r\n";
				}
			}
			catch (const std::exception&)
			{
				WriteLog("��־д���ļ�ʧ��!�ȴ�5�����:" + remark);
				std::this_thread::sleep_for(std::chrono::seconds(5));//�ӳ�5��
				continue;
			}
			m_pLogQueue->iTail = (m_pLogQueue->iTail + 1) % m_pLogQueue->nQueueLen;
		}
		else
		{
			for (auto item = m_mpModInfo.begin(); item != m_mpModInfo.end(); item++)
			{
				item->second->flush();
			}
		}
	}
}

//д��־�߳�(���ݿ�)
void CB_WriteLog::TrueWriteLog2DBThread()
{
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));//�ӳ�1����
		//д�����ݵ����ݿ�
	}
}

//��ȡlog·��
std::string	CB_WriteLog::GetLogPath()
{
	return m_sFilePath;
}

//��ȡlog�ָ���
std::string	CB_WriteLog::GetLogDelimiter()
{
	return	m_sDelimiter;
}

//��ȡlog���ݿ������ַ���
std::string	CB_WriteLog::GetLogDBStr()
{
	return m_sDB;
}

//��ȡlog����ȼ�
int	CB_WriteLog::GetLogLevel()
{
	return m_iSaveLevel;
}

//��ȡlog����ʱ��
int	CB_WriteLog::GetLogSaveDays()
{
	return m_iExpireDays;
}

//��ȡlog�����С
int	CB_WriteLog::GetLogBufferSize()
{
	return m_iBufferSize;
}

//��ȡlog�ļ�λ�ã�����ָ��ʱ�䣨ʱ���ʽ��%d-%d-%d����ģ��
std::string	CB_WriteLog::GetLogFromFile(const std::string sDate, const std::string sModName)
{
	std::string sFilePath = "";
	std::string sTemp = "";
	if ("" == sDate && "" != sModName)
	{
		for (auto item = m_mpSaveDate.begin(); item != m_mpSaveDate.end(); item++)
		{
			sTemp = m_sFilePath + "//" + item->first + "//" + sModName + ".txt";
			if (!access(sTemp.c_str(), 0))
			{
				sFilePath.append(sTemp).append(m_sDelimiter);
			}
		}
	}
	else if ("" != sDate && "" == sModName)
	{
		sTemp = m_sFilePath + "//" + sDate;
		std::vector < std::string > vListLogFile;
		GetListFiles(sTemp, vListLogFile);
		for (auto iter = vListLogFile.begin(); iter != vListLogFile.end(); ++iter)
		{
			sTemp.append(*iter);
			sFilePath.append(sTemp).append(m_sDelimiter);
		}
	}
	else if ("" != sDate && "" != sModName)
	{
		sTemp = m_sFilePath + "//" + sDate + "//" + sModName + ".txt";
		if (!access(sTemp.c_str(), 0))
		{
			sFilePath.append(sTemp);
		}
	}

	return sFilePath;
}

//�����ݿ��л�ȡlog�����ɶ�Ӧlog�ļ��������ļ�λ��, ��Ҫ�ṩʱ�����䣨��ʼ�ͽ�������ģ���б��ȼ��б�
std::string	CB_WriteLog::GetLogFromDB(const std::string sStartDate, const std::string sEndDate, const std::vector<std::string> vModName, const std::vector<int> vLevel)
{
	//��װsql���
	//ʹ��sql
	//�õ��ṹ������
	return "";
}