#include <iostream>
#include <list>
#include <tchar.h>
#include <time.h>
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")

#include <Windows.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include "md5.h"

using namespace std;

#define SERVER_IP "192.168.0.4"
#define SERVER_PORT 12345

#define TEST_FILE_PATH "E:\\"
#define MAX_CHECK_TIME 300
#define FILE_BUF_SIZE 4096

typedef struct sItemFileMd5 {
	char *szFilePath;
	md5_byte_t b_digHashMD5[16];
	md5_byte_t a_digHashMD5[16];
	bool isChanged;
} ITEM_FILE_MD5;

list<ITEM_FILE_MD5> g_listFileMD5;
bool g_isFirstShow = true;

bool GenerateMD5(md5_byte_t *md5_out, unsigned char *buf, unsigned int buf_size)
{
	md5_state_t state;

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)buf, buf_size);
	md5_finish(&state, md5_out);

	return true;
}

bool GetFileMD5Hash(char *szFilePath, md5_byte_t *md5_out, long *nSize)
{
	FILE* fpTarget;
	long file_size;
	int nReadBytes = 0;
	int nTotalReadBytes = 0;
	unsigned char* buf;

	fpTarget = fopen(szFilePath, "rb");
	if (fpTarget == NULL)
		return false;

	fseek(fpTarget, 0, SEEK_END);
	file_size = ftell(fpTarget);
	buf = new unsigned char[file_size];
	fseek(fpTarget, 0, SEEK_SET);

	while ((nReadBytes = fread(buf + nTotalReadBytes, sizeof(char), 4, fpTarget))>0) {
		nTotalReadBytes += nReadBytes;
	}

	fclose(fpTarget);

	GenerateMD5(md5_out, buf, file_size); // MD5 생성
	delete buf;

	*nSize = file_size;
	return true;
}

int FindFiles(char *szFilePath)
{
	HANDLE hFind;
	WIN32_FIND_DATA fd = { 0 };
	char szFindFilter[MAX_PATH + 1] = { 0 };
	char szSrcFile[MAX_PATH + 1] = { 0 };
	char szNewPath[MAX_PATH + 1] = { 0 };
	int nCount = 0;

	sprintf(szFindFilter, _T("%s\\*.*"), szFilePath);
	hFind = ::FindFirstFile(szFindFilter, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (fd.cFileName[0] == '.')
				continue;
			sprintf(szSrcFile, _T("%s%s"), szFilePath, fd.cFileName);
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				strcat(szSrcFile, "\\");
				nCount += FindFiles(szSrcFile);
			}
			else {
				nCount++;
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	return nCount;
}

bool InitFileCaching()
{
	int nCount = 0;
	nCount = FindFiles("c:\\");
	return true;
}

bool GetTestFile(char *szFileName)
{
	HANDLE hFind;
	HANDLE hFile;
	WIN32_FIND_DATA fd = { 0 };
	char szFindFilter[1024 + 1] = { 0 };
	char szSrcFile[1024 + 1] = { 0 };
	bool isFindFile = false;
	md5_byte_t digHashMD5[16];
	char szHexMD5[32 + 1] = { 0 };
	long file_size;

	cout << "=============================================================================" << endl;
	sprintf(szFindFilter, _T("%s\\*.exe"), TEST_FILE_PATH);
	hFind = ::FindFirstFile(szFindFilter, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (fd.cFileName[0] == '.')
				continue;
			sprintf(szSrcFile, _T("%s%s"), TEST_FILE_PATH, fd.cFileName);
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				cout << " - Test File : " << szSrcFile << endl;
				isFindFile = true;
			}
			break;
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	if (isFindFile == false) {
		cout << "[ERROR] Not Found - Test File" << endl;
		return false;
	}

	// Get File Info
	GetFileMD5Hash(szSrcFile, digHashMD5, &file_size);
	cout << " - File Size : " << file_size / 1024 << "KB" << endl;

	// Show MD5
	for (int i = 0; i < 16; i++)
		sprintf(szHexMD5 + i * 2, "%02X", digHashMD5[i]);
	cout << " - File Hash : " << szHexMD5 << " [MD5]" << endl;

	strncpy(szFileName, szSrcFile, 1024);
	return true;
}

bool SetTargetFiles()
{
	FILE* fpList;
	int size;
	long file_size;
	char szFilePath[1024];
	md5_byte_t digHashMD5[16];
	char szHexMD5[32 + 1] = { 0 };
	ITEM_FILE_MD5 tmpFileMD5;

	cout << "=============================================================================" << endl;
	cout << " - Target Files" << endl;

	fpList = fopen("listTarget.txt", "r");
	if (fpList == NULL) {
		cout << "[ERROR] Not Found - List File" << endl;
		return false;
	}

	while (size = fscanf(fpList, "%s", &szFilePath)) {
		if ((int)size <= 0) break;
		if (GetFileMD5Hash(szFilePath, digHashMD5, &file_size) == false)
			continue;

		// Show MD5
		for (int i = 0; i < 16; i++)
			sprintf(szHexMD5 + i * 2, "%02X", digHashMD5[i]);
		cout << "[MD5] " << szHexMD5 << " - " << szFilePath << endl;

		// Add List
		tmpFileMD5.szFilePath = new char[strlen(szFilePath)+1];
		strncpy(tmpFileMD5.szFilePath, szFilePath, strlen(szFilePath) + 1);
		memcpy(tmpFileMD5.b_digHashMD5, digHashMD5, 16);
		tmpFileMD5.isChanged = false;

		g_listFileMD5.push_back(tmpFileMD5);
	}

	cout << "=============================================================================" << endl;

	fclose(fpList);
	return true;
}

bool ByteToHexString(unsigned char *buf, char *hexString, int len)
{
	for (int i = 0; i < len; i++)
		sprintf(hexString + i * 2, "%02X", buf[i]);
	return true;
}

void SecToTime(int sec, char *szTimeText)
{
	int m_secTotal;
	int m_nHour;
	int m_nMin;
	int m_nSec;

	if (sec < 0) sec = 0;
	m_secTotal = sec;
	m_nHour = m_secTotal / 3600;
	m_nMin = (m_secTotal % 3600) / 60;
	m_nSec = (m_secTotal % 3600) % 60;

	sprintf(szTimeText, "%02d:%02d:%02d", m_nHour, m_nMin, m_nSec);
	return;
}

bool CheckFileIntegrity()
{
	bool result = true;
	long file_size;
	char szHexMD5[32 + 1] = { 0 };
	list<ITEM_FILE_MD5>::iterator itor = g_listFileMD5.begin();
	md5_byte_t digHashMD5[16] = { 0 };
	
	while (itor != g_listFileMD5.end()) {
		if (GetFileMD5Hash(itor->szFilePath, digHashMD5, &file_size) == false) {
			if (itor->isChanged == false) {
				ZeroMemory(digHashMD5, 16);
				memcpy(itor->a_digHashMD5, digHashMD5, 16);
				itor->isChanged = true;
				result = false;
			}
		}else{
			if (memcmp(digHashMD5, itor->b_digHashMD5, 16) != 0) {
				memcpy(itor->a_digHashMD5, digHashMD5, 16);
				if (itor->isChanged == false) {
					itor->isChanged = true;
					result = false;
				}
			}
		}
		itor++;
	}

	if (g_isFirstShow || !result) {
		system("cls");
		cout << "=============================================================================" << endl;
		itor = g_listFileMD5.begin();
		while (itor != g_listFileMD5.end()) {
			if (!itor->isChanged) {
				ByteToHexString(itor->b_digHashMD5, szHexMD5, 16);
				cout << "○ [MD5] " << szHexMD5 << endl;
			}
			else {
				ByteToHexString(itor->b_digHashMD5, szHexMD5, 16);
				cout << "● [MD5] " << szHexMD5 << " → ";
				ByteToHexString(itor->a_digHashMD5, szHexMD5, 16);
				cout << szHexMD5 << endl;
			}
			itor++;
		}
		cout << "=============================================================================" << endl;
		g_isFirstShow = false;
	}

	return result;
}

bool SendTestResult(char *szTestName, bool isDetected)
{
	WSADATA wsaData;
	SOCKET m_hSocket;
	SOCKADDR_IN servAdr;
	char buf[1024] = { 0 };

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup() error!" << endl;
		return false;
	}

	m_hSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (m_hSocket == INVALID_SOCKET) {
		cout << "socket() error" << endl;
		return false;
	}

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = inet_addr(SERVER_IP);
	servAdr.sin_port = htons(SERVER_PORT);

	if (connect(m_hSocket, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR) {
		cout << "connect() error!" << endl;
		return false;
	}

	//cout << "Connected!" << endl;
	sprintf(buf, "ResultVM %s %d", szTestName, isDetected);
	send(m_hSocket, buf, strlen(buf), 0);
	cout << "Report completed!" << endl;

	closesocket(m_hSocket);
	WSACleanup();
	return true;
}

int main(int argc, char* argv[])
{
	bool result;
	int nResult = 0;
	char szFileName[1024] = { 0 };
	STARTUPINFO StartupInfo = { 0 };
	PROCESS_INFORMATION ProcessInfo;
	StartupInfo.cb = sizeof(STARTUPINFO);
	clock_t t_start, t_end;
	char szTemp[1024] = { 0 };
	bool isDetected = false;
	char szItemName[1024] = { 0 };

	if (argc >= 2) {
		if (strcmp(argv[1], "init") == 0) {
			InitFileCaching();
			return 0;
		}
	}

	cout << "VM Analyzer - Integrity check" << endl;

	// 테스트 실행 파일명 확인
	if (GetTestFile(szFileName) == false) {
		cout << "[ERROR] Failed - GetTestFile()" << endl;
		return 1;
	}

	// 감시 파일 목록 생성
	if (SetTargetFiles() == false) {
		return 1;
	}

	// 실행
	cout << "Execute: " << szFileName << endl;
	::CreateProcess(szFileName, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &StartupInfo, &ProcessInfo);
	cout << "Waiting..." << endl;

	// 검사
	t_start = clock();
	do {
		Sleep(10000);
		if (CheckFileIntegrity() == false) {
			if (isDetected == false) {
				t_start = clock() - ((MAX_CHECK_TIME - 60) * CLOCKS_PER_SEC);
				isDetected = true;
			}
		}
		t_end = clock();
		SecToTime((int)MAX_CHECK_TIME - ((float)(t_end - t_start) / CLOCKS_PER_SEC), szTemp);
		cout << "\r - " << szTemp;
	} while (((float)(t_end - t_start) / CLOCKS_PER_SEC) < MAX_CHECK_TIME);
	cout << endl;

	// 파일명 분리
	strncpy(szItemName, szFileName, PathFindExtension(szFileName) - szFileName);
	PathStripPath(szItemName);
	cout << szItemName << endl;

	// 결과 전송
	result = SendTestResult(szItemName, isDetected);
	if (result == false) {
		cout << "[ERROR] Failed - SendTestResult()" << endl;
		return 1;
	}

	return 0;
}