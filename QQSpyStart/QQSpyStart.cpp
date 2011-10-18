#include <fstream>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <process.h>
#include <psapi.h>

static std::vector<std::string> list;
static std::string runexe;
static std::string srcpath;

static void Install(bool bStart);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	if(__argc==1)
	{
		Install(false);
		return 0;
	}

	char curdir[MAX_PATH];
	GetModuleFileNameA(NULL, curdir, sizeof(curdir));
	*strrchr(curdir, '\\') = '\0';
	SetCurrentDirectoryA(curdir);

	for(int i=1; i<__argc; i++)
	{
		char argv[10000];
		strcpy(argv, __argv[i]);
		char* pos = strchr(argv, '=');
		if(!pos)
		{
			pos = "";
		}
		else
		{
			*(pos++) = '\0';
		}

		if(strcmp(argv, "srcpath")==0)
		{
			srcpath = pos;
			continue;
		}
		if(strcmp(argv, "file")==0)
		{
			if(strlen(pos)>0)
			{
				list.push_back(pos);
			}
			continue;
		}
		if(strcmp(argv, "runexe")==0)
		{
			runexe = pos;
			continue;
		}
		if(strcmp(argv, "kill")==0)
		{
			if(strlen(pos)>0)
			{
				char cmd[1000];
				sprintf(cmd, "taskkill /F /IM %s", pos);
				system(cmd);
			}
			continue;
		}
	}

	if(!runexe.empty())
	{
		DWORD dwPIDs[1000];
		DWORD dwSize;
		if(EnumProcesses(dwPIDs, sizeof(dwPIDs), &dwSize))
		{
			char exepath[MAX_PATH];
			sprintf(exepath, "%s\\%s", curdir, runexe.c_str());
			for(DWORD i=0; i<dwSize/sizeof(DWORD); i++)
			{
				HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPIDs[i]);
				if(hProcess)
				{
					char file[MAX_PATH];
					if(GetModuleFileNameExA(hProcess, NULL, file, sizeof(file))>0)
					{
						if(stricmp(file, exepath)==0)
						{
							TerminateProcess(hProcess, 0);
						}
					}
					CloseHandle(hProcess);
				}
			}
		}
	}

	try
	{
		if(!srcpath.empty())
		{
			std::ifstream in;
			std::ofstream out;
			char path[MAX_PATH];
			std::vector<std::string>::iterator i;

			for(i=list.begin(); i!=list.end(); i++)
			{
				sprintf(path, "%s%s", srcpath.c_str(), (*i).c_str());
				in.open(path, std::ios::in|std::ios::binary);
				if(!in) throw "";
				out.open(L"tmp.tmp", std::ios::out|std::ios::binary|std::ios::trunc);
				if(!out) throw "";
				out << in.rdbuf();
				out.close();
				in.close();

				in.open(L"tmp.tmp", std::ios::in|std::ios::binary);
				if(!in) throw "";
				out.open((*i).c_str(), std::ios::out|std::ios::binary|std::ios::trunc);
				if(!out) throw "";
				out << in.rdbuf();
				out.close();
				in.close();
			}
		}
	}
	catch(...)
	{
	}

	if(!runexe.empty())
	{
		STARTUPINFOA si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOW;
		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));
		if(!CreateProcessA(runexe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		{
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
	}

	return 0;
}

static void Install(bool bStart)
{
	char* source	= "$$$$\\\\192.168.5.11\\Tools\\QQMon\\QQStart.exe\0$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$";
	char* args		= "####srcpath=\\\\192.168.5.11\\Tools\\QQMon\\ file=QQMon.exe file=QQMon.start runexe=QQMon.exe\0############################################################";
	char* target	= "****QQMon\0************************************************************************************************************************************************";

	// make directory
	char path[MAX_PATH];
	sprintf(path, "%s%s\\%s", getenv("HOMEDRIVE"), getenv("HOMEPATH"), target+4);
	CreateDirectoryA(path, NULL);
	SetCurrentDirectoryA(path);

	// copy file
	char file[MAX_PATH];
	sprintf(file, "%s\\%s", path, strrchr(source, '\\')+1);
	try
	{
		std::wifstream in;
		std::wofstream out;
		in.open(source+4, std::ios::in|std::ios::binary);
		out.open(file, std::ios::out|std::ios::binary|std::ios::trunc);
		out << in.rdbuf();
		out.close();
		in.close();
	}
	catch(...)
	{
	}

	// add to start menu
	char cmd[1000];
	sprintf(cmd, "\"%s\" %s", file, args+4);
	RegSetKeyValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", target+4, REG_SZ, cmd, strlen(cmd)+1);

	// start
	if(bStart)
	{
		STARTUPINFOA si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOW;
		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));
		if(!CreateProcessA(file, args+4, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		{
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
	}
}
