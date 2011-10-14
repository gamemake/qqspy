#include <fstream>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <process.h>

static std::vector<std::string> list;
static std::string runexe;
static std::string srcpath;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
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
