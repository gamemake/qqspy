#include <stdio.h>
#include <math.h>
#include <time.h>
#include <malloc.h>
#include <winsock2.h>
#include <Windows.h>
#include <GdiPlus.h>
#include <psapi.h>
#include <DbgHelp.h>

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num = 0, size = 0;
	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if(size==0) return -1;

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if(pImageCodecInfo==NULL) return -1;

	GetImageEncoders(num, size, pImageCodecInfo);

	for(UINT j = 0; j < num; ++j)
	{
		if(wcscmp(pImageCodecInfo[j].MimeType, format)==0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;
		}        
	}

	free(pImageCodecInfo);
	return -1;
} 

int Xmemcmp(const byte* a, const byte* b, int size)
{
	int count = 0;
	for(int i=0; i<size; i++)
	{
		if(a[i]!=b[i]) count++;
	}
	return count;
}

struct CHECK_ITEM
{
	CHECK_ITEM()
	{
		wcscpy(name, L"UNKNOWN");
		count = 0;
		old_p = NULL;
	}
	wchar_t name[100];
	int count;
	byte* old_p;
	int old_width, old_height, old_bitcount;
};

bool ScreenToBMP(const wchar_t* pFileName, HWND hWnd, CHECK_ITEM* item)
{
	int nX, nY, nWidth, nHeight;
	RECT rct;
	GetWindowRect(hWnd, &rct);
	nX = rct.left + 10;
	nY = rct.top  + 10 + 80;
	nWidth = rct.right   - rct.left - 20 - 150;
	nHeight = rct.bottom - rct.top  - 20 - 80 - 130;

	HDC hScreenDC = CreateDCW(L"DISPLAY", NULL, NULL, NULL);// GetDC(NULL);
	HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, nWidth, nHeight);
	HDC hMemDC = CreateCompatibleDC(hScreenDC);
	HGDIOBJ hOldBitmap = SelectObject(hMemDC, hBitmap);
	BitBlt(hMemDC, 0, 0, nWidth, nHeight, hScreenDC, nX, nY, SRCCOPY);
	int nBitCount = GetDeviceCaps(hMemDC, BITSPIXEL);

	BITMAPINFOHEADER bih = {0};
	bih.biBitCount = nBitCount;
	bih.biCompression = BI_RGB;
	bih.biWidth = nWidth;
	bih.biHeight = nHeight;
	bih.biPlanes = 1;
	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biSizeImage = bih.biBitCount / 8 * nWidth * nHeight;

	byte * p = new byte[nBitCount / 8 * nWidth * nHeight];
	GetDIBits(hMemDC, hBitmap, 0, nHeight, p, (LPBITMAPINFO) &bih, DIB_RGB_COLORS);
	if(item->old_p)
	{
		if(item->old_width==nWidth && item->old_height==nHeight && item->old_bitcount==nBitCount)
		{
			int x = Xmemcmp(p, item->old_p, nBitCount/8*nWidth*nHeight) / 4;
			if(x<100)
			{
				delete p;
				return false;
			}
		}
	}
	item->old_p = p;
	item->old_width = nWidth;
	item->old_height = nHeight;
	item->old_bitcount = nBitCount;

	Gdiplus::Bitmap b(hBitmap, NULL);
	static CLSID pngClsid;
	static bool init = false;
	if(!init)
	{
		GetEncoderClsid(L"image/jpeg", &pngClsid);
		init = true;
	}
	b.Save(pFileName, &pngClsid);

	SelectObject(hMemDC, hOldBitmap);
	DeleteDC(hMemDC);
	DeleteObject(hBitmap);
	DeleteDC(hScreenDC);

	return true;
} 

bool PointInRect(int x, int y, const RECT& rct)
{
	return x>rct.left && x<rct.right && y>rct.top && y<rct.bottom;
}

bool RectOverlaped(const RECT& rcta, const RECT& rctb)
{
	if(PointInRect(rcta.left,  rcta.top,    rctb)) return true;
	if(PointInRect(rcta.left,  rcta.bottom, rctb)) return true;
	if(PointInRect(rcta.right, rcta.top,    rctb)) return true;
	if(PointInRect(rcta.right, rcta.bottom, rctb)) return true;
	if(PointInRect(rctb.left,  rctb.top,    rcta)) return true;
	if(PointInRect(rctb.left,  rctb.bottom, rcta)) return true;
	if(PointInRect(rctb.right, rctb.top,    rcta)) return true;
	if(PointInRect(rctb.right, rctb.bottom, rcta)) return true;
	return false;
}

static bool RectIn(const RECT& rcta, const RECT& rctb)
{
	if(!PointInRect(rcta.left,  rcta.top,    rctb)) return false;
	if(!PointInRect(rcta.left,  rcta.bottom, rctb)) return false;
	if(!PointInRect(rcta.right, rcta.top,    rctb)) return false;
	if(!PointInRect(rcta.right, rcta.bottom, rctb)) return false;
	return true;
}

HWND GetWindowByName(const wchar_t* pName)
{
	HWND hWnd = GetWindow(GetDesktopWindow(), GW_CHILD);
	while(hWnd)
	{
		wchar_t szValue[1000];
		szValue[0] = '\0';
		GetWindowTextW(hWnd, szValue, sizeof(szValue));
		if(wcscmp(pName, szValue)==0)
		{
			if(IsWindowVisible(hWnd) && (GetWindowLong(hWnd, GWL_STYLE)&WS_MINIMIZE)==0)
			{
				RECT rcta, rctb;
				GetWindowRect(hWnd, &rcta);
				HWND hPrevWnd = GetWindow(hWnd, GW_HWNDPREV);
				while(hPrevWnd)
				{
					if(IsWindowVisible(hPrevWnd) && (GetWindowLong(hPrevWnd, GWL_STYLE)&WS_MINIMIZE)==0)
					{
						GetWindowRect(hPrevWnd, &rctb);
						if(RectIn(rcta, rctb))
							break;
					}
					hPrevWnd = GetWindow(hPrevWnd, GW_HWNDPREV);
				}
				if(!hPrevWnd) return hWnd;
			}
			return NULL;
		}
		hWnd = GetWindow(hWnd, GW_HWNDNEXT);
	}
	return NULL;
}

LONG WINAPI IceUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	static BOOL fFirstShot = TRUE;
	if(!fFirstShot)
	{
		return(EXCEPTION_CONTINUE_SEARCH);
	}
	fFirstShot = FALSE;

	TCHAR szCmdLine[MAX_PATH+1024];
	GetModuleFileName(NULL, szCmdLine, MAX_PATH);
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if(!CreateProcess(szCmdLine, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}

	wchar_t szDmpPath[MAX_PATH];
	GetModuleFileNameW(NULL, szDmpPath, sizeof(szDmpPath));
	LPWSTR pc = szDmpPath;
	while(*pc) ++pc;
	while(*pc!='.') --pc;
	DWORD pid, i = 10;
	pid = GetCurrentProcessId();
	for(; i>1; i--)
	{
		DWORD x = (DWORD)pow((double)10, (double)(i-1));
		if((pid / x % 10) != 0) break;
	}
	for(; i>0; i--)
	{
		DWORD x = (DWORD)pow((double)10, (double)(i-1));
		*(++pc) = L'0' + (pid / x % 10);
	}
	*(++pc) = L'.';
	*(++pc) = L'd';
	*(++pc) = L'm';
	*(++pc) = L'p';
	*(++pc) = L'\0';

	HANDLE hDmp = CreateFileW(szDmpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH, NULL);
	if(hDmp==INVALID_HANDLE_VALUE) return(EXCEPTION_CONTINUE_SEARCH);
	if(ExceptionInfo)
	{
		MINIDUMP_EXCEPTION_INFORMATION mdei;
		mdei.ThreadId = GetCurrentThreadId();
		mdei.ExceptionPointers = ExceptionInfo;
		mdei.ClientPointers = FALSE;
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDmp, MiniDumpNormal, &mdei, NULL, NULL);
	}
	else
	{
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDmp, MiniDumpNormal, NULL, NULL, NULL);
	}
	CloseHandle(hDmp);

	return(EXCEPTION_EXECUTE_HANDLER);
}

void IceInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
{
	IceUnhandledExceptionFilter(NULL);
	exit(0);
}

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <sstream>
using namespace std;

static map<wstring, void(*)(vector<wstring>&)> g_Cmds;

void NetSendLine(const wchar_t* line);
void NetSendData(const char* val, int len);

void DoCmd(const wchar_t* _cmd)
{
	wstring cmd = _cmd;

	// trim(cmd)
	{
		wstring::iterator   p1=find_if(cmd.begin(),cmd.end(),not1(ptr_fun(isspace)));   
		cmd.erase(cmd.begin(), p1);
		wstring::reverse_iterator  p2=find_if(cmd.rbegin(), cmd.rend(),not1(ptr_fun(isspace)));   
		cmd.erase(p2.base(), cmd.end());
	}
	
	vector<wstring> args;
	// args = split(cmd, ' ')
	{
		wistringstream iss(cmd);
		wstring c;
		while(iss >> c)
		{
			args.push_back(c);
        }
	}

	if(args.size()==0)
	{
		NetSendData(":", 1);
		return;
	}

	map<wstring, void(*)(vector<wstring>&)>::iterator i;
	i = g_Cmds.find(args.front());
	if(i!=g_Cmds.end())
	{
		i->second(args);
	}
	else
	{
		NetSendLine(L"cmd not found.");
	}

	NetSendData(":", 1);
}

static SOCKET g_hListen = INVALID_SOCKET;
static SOCKET g_hClient = INVALID_SOCKET;
static char g_ClientBuf[10000];
static int g_ClientBufSize = 0;

bool NetBind(DWORD ip, WORD port)
{
	if(g_hListen!=INVALID_SOCKET) return false;
	g_hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(g_hListen==INVALID_SOCKET) return false;

	sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = ip;
	sa.sin_port = htons(port);

	if(::bind(g_hListen, (SOCKADDR*)&sa, sizeof(sa))==SOCKET_ERROR || listen(g_hListen, SOMAXCONN)==SOCKET_ERROR)
	{
		closesocket(g_hListen);
		g_hListen = INVALID_SOCKET;
		return false;
	}

	return true;
}

void NetTickListen(DWORD time)
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(g_hListen, &readfds);
	timeval timeout;
	timeout.tv_sec = time / 1000;
	timeout.tv_usec = (time%1000) * 1000;

	if(select(0, &readfds, NULL, NULL, &timeout)!=1) return;
	g_hClient = accept(g_hListen, NULL, NULL);
	if(g_hClient==INVALID_SOCKET) return;
	g_ClientBufSize = 0;

	NetSendLine(L"Welcome to QQMON!");
	NetSendData(":", 1);
}

void NetTickClient(DWORD time)
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(g_hClient, &readfds);
	timeval timeout;
	timeout.tv_sec = time / 1000;
	timeout.tv_usec = (time%1000) * 1000;

	if(select(0, &readfds, NULL, NULL, &timeout)!=1) return;
	int ret = recv(g_hClient, g_ClientBuf+g_ClientBufSize, sizeof(g_ClientBuf)-g_ClientBufSize, 0);
	if(ret<=0)
	{
		shutdown(g_hClient, SD_BOTH);
		closesocket(g_hClient);
		g_hClient = INVALID_SOCKET;
		return;
	}
	g_ClientBufSize += ret;

	// parse
	char* pos = strstr(g_ClientBuf, "\r\n");
	if(!pos) return;
	int len = pos - g_ClientBuf;
	*pos = '\0';

	wchar_t string[2000];
	ret = MultiByteToWideChar(CP_ACP, 0, g_ClientBuf, len, string, (int)sizeof(string)/sizeof(wchar_t));
	string[ret] = L'\0';
	DoCmd(string);

	memmove(g_ClientBuf, g_ClientBuf+len+2, g_ClientBufSize-len-2);
	g_ClientBufSize -= len + 2;
}

void NetTick(DWORD time)
{
	if(g_hListen==INVALID_SOCKET)
	{
		Sleep(time);
		return;
	}

	DWORD dwStart = GetTickCount();
	if(g_hClient!=INVALID_SOCKET)
	{
		NetTickClient(time);
	}
	else
	{
		NetTickListen(time);
	}

	DWORD dwEnd = GetTickCount();
	if(dwEnd-dwStart<time)
	{
		NetTick(time - (dwEnd-dwStart));
	}
}

void NetSendData(const char* val, int len)
{
	int slen = 0;
	while(1)
	{
		int ret = send(g_hClient, val+slen, len-slen, 0);
		if(ret<=0)
		{
			closesocket(g_hClient);
			g_hClient = INVALID_SOCKET;
			return;
		}
		slen += ret;
		if(slen>=len) break;
	}
}

void NetSendLine(const wchar_t* line)
{
	if(g_hClient==INVALID_SOCKET) return;

	char val[10000];
	int len = WideCharToMultiByte(CP_ACP, 0, line, wcslen(line), val, sizeof(val), NULL, NULL);
	strcpy(val+len, "\r\n");

	NetSendData(val, len+2);
}

void NetUnbind()
{
	if(g_hClient!=INVALID_SOCKET) closesocket(g_hClient);
	if(g_hListen!=INVALID_SOCKET) closesocket(g_hListen);
	g_hClient = INVALID_SOCKET;
	g_hListen = INVALID_SOCKET;
}

#include <fstream>

static LPTOP_LEVEL_EXCEPTION_FILTER g_oldUEF;
static CHECK_ITEM g_List[100];
static int g_ListCount = 0;
static wchar_t g_SavePath[MAX_PATH] = L"";
static void Cmd_screenshot(vector<wstring>& args);
static void Cmd_append(vector<wstring>& args);
static void Cmd_delete(vector<wstring>& args);
static void Cmd_list(vector<wstring>& args);
static void Cmd_savepath(vector<wstring>&args);
static void Cmd_netbind(vector<wstring>& args);
static void Cmd_netunbind(vector<wstring>& args);
static void Cmd_copyfile(vector<wstring>& args);
static void Cmd_shell(vector<wstring>& args);
static void Cmd_exec(vector<wstring>& args);
static void Cmd_close(vector<wstring>& args);
static void Cmd_kill(vector<wstring>& args);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);
	setlocale(LC_ALL, ".ACP");

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	time_t t = time(NULL);

	g_oldUEF = SetUnhandledExceptionFilter(IceUnhandledExceptionFilter);
	_set_invalid_parameter_handler(IceInvalidParameterHandler);

	g_Cmds[L"screenshot"]	= Cmd_screenshot;
	g_Cmds[L"append"]		= Cmd_append;
	g_Cmds[L"delete"]		= Cmd_delete;
	g_Cmds[L"list"]			= Cmd_list;
	g_Cmds[L"savepath"]		= Cmd_savepath;
	g_Cmds[L"netbind"]		= Cmd_netbind;
	g_Cmds[L"netunbind"]	= Cmd_netunbind;
	g_Cmds[L"copyfile"]		= Cmd_copyfile;
	g_Cmds[L"shell"]		= Cmd_shell;
	g_Cmds[L"exec"]			= Cmd_exec;
	g_Cmds[L"close"]		= Cmd_close;
	g_Cmds[L"kill"]			= Cmd_kill;

	//
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	*strrchr(path, '.') = '\0';
	strcat(path, ".start");
	try
	{
		ifstream in(path);
		if(!in)
		{
			in.open("QQMon.start");
			if(!in) throw "";
		}
		char linea[1000];
		wchar_t linew[1000];
		while(in.getline(linea, sizeof(linea)))
		{
			int ret = MultiByteToWideChar(CP_ACP, 0, linea, strlen(linea), linew, (int)sizeof(linew)/sizeof(wchar_t));
			linew[ret] = L'\0';
			DoCmd(linew);
		}
		in.close();
	}
	catch(...)
	{
	}

	while(1)
	{
		NetTick(1000);

		for(int i=0; i<g_ListCount; i++)
		{
			wchar_t fn[1000];
			swprintf(fn, L"%s%s-%I64d-%010d.jpeg", g_SavePath, g_List[i].name, t, g_List[i].count);
			HWND hWnd = GetWindowByName(g_List[i].name);
			if(hWnd && ScreenToBMP(fn, hWnd, &g_List[i]))
			{
				g_List[i].count++;
			}
		}
	}
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return 0;
}

void Cmd_screenshot(vector<wstring>& args)
{
	if(args.size()!=2)
	{
		NetSendLine(L"invalid parameter");
		return;
	}

	int nWidth, nHeight;
	HDC hScreenDC = CreateDCW(L"DISPLAY", NULL, NULL, NULL);// GetDC(NULL);
	nWidth = GetDeviceCaps(hScreenDC, HORZRES);
	nHeight = GetDeviceCaps(hScreenDC, VERTRES);
	HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, nWidth, nHeight);
	HDC hMemDC = CreateCompatibleDC(hScreenDC);
	HGDIOBJ hOldBitmap = SelectObject(hMemDC, hBitmap);
	BitBlt(hMemDC, 0, 0, nWidth, nHeight, hScreenDC, 0, 0, SRCCOPY);

	Gdiplus::Bitmap b(hBitmap, NULL);
	static CLSID pngClsid;
	static bool init = false;
	if(!init)
	{
		GetEncoderClsid(L"image/jpeg", &pngClsid);
		init = true;
	}
	if(b.Save(args[1].c_str(), &pngClsid)==Gdiplus::Ok)
	{
		NetSendLine(L"done.");
	}
	else
	{
		NetSendLine(L"error in write file");
	}

	SelectObject(hMemDC, hOldBitmap);
	DeleteDC(hMemDC);
	DeleteObject(hBitmap);
	DeleteDC(hScreenDC);
}

void Cmd_append(vector<wstring>& args)
{
	if(args.size()!=2)
	{
		NetSendLine(L"invalid parameter");
		return;
	}

	if(g_ListCount>=sizeof(g_List)/sizeof(g_List[0]))
	{
		NetSendLine(L"too many.");
		return;
	}

	for(int i=0; i<g_ListCount; i++)
	{
		if(wcscmp(g_List[i].name, args[1].c_str())==0)
		{
			NetSendLine(L"already existed.");
			return;
		}
	}

	wcscpy(g_List[g_ListCount].name, args[1].c_str());
	g_List[g_ListCount].old_p = NULL;
	g_ListCount++;

	NetSendLine(L"done.");
}

void Cmd_delete(vector<wstring>& args)
{
	if(args.size()!=2)
	{
		NetSendLine(L"invalid parameter");
		return;
	}

	for(int i=0; i<g_ListCount; i++)
	{
		if(wcscmp(g_List[i].name, args[1].c_str())==0)
		{
			if(g_List[i].old_p) delete [] g_List[i].old_p;
			memmove(g_List+i, g_List+i+1, sizeof(g_List[0])*(g_ListCount-i-1));
			g_ListCount--;
			NetSendLine(L"done.");
			return;
		}
	}

	NetSendLine(L"not found.");
}

void Cmd_list(vector<wstring>& args)
{
	for(int i=0; i<g_ListCount; i++)
	{
		wchar_t line[100];
		swprintf(line, sizeof(line), L"%03d %s", i, g_List[i].name);
		NetSendLine(line);
	}

	NetSendLine(L"done");
}

void Cmd_savepath(vector<wstring>& args)
{
	if(args.size()<2)
	{
		wcscpy(g_SavePath, L"");
	}
	else
	{
		wcscpy(g_SavePath, args[1].c_str());
	}
	NetSendLine(L"done.");
}

void Cmd_netbind(vector<wstring>& args)
{
	SOCKADDR_IN sa;
	INT salen = sizeof(sa);
	if(args.size()!=2 || WSAStringToAddressW((LPWSTR)args[1].c_str(), AF_INET, NULL, (LPSOCKADDR)&sa, &salen)==SOCKET_ERROR)
	{
		NetSendLine(L"invalid parameter");
		return;
	}
	if(g_hListen!=INVALID_SOCKET)
	{
		NetUnbind();
	}
	if(!NetBind(sa.sin_addr.S_un.S_addr, ntohs(sa.sin_port)))
	{
		NetSendLine(L"failed to bind");
		return;
	}
	NetSendLine(L"done.");
}

void Cmd_netunbind(vector<wstring>& args)
{
	NetUnbind();
	NetSendLine(L"done.");
}

void Cmd_copyfile(vector<wstring>& args)
{
	if(args.size()!=3)
	{
		NetSendLine(L"invalid parameter");
		return;
	}

	std::wifstream in;
	std::wofstream out;

	try
	{
		in.open(args[1].c_str(), ios::in|ios::binary);
		if(!in) throw "";
	}
	catch(...)
	{
		NetSendLine(L"failed to open source file");
		return;
	}
	try
	{
		out.open(args[2].c_str(), ios::out|ios::binary|std::ios::trunc);
		if(!in) throw "";
	}
	catch(...)
	{
		in.close();
		NetSendLine(L"failed to open target file");
		return;
	}

	try
	{
		out << in.rdbuf();
	}
	catch(...)
	{
		out.close();
		in.close();
		NetSendLine(L"error...");
	}

	out.close();
    in.close();
	NetSendLine(L"done.");
}

void Cmd_shell(vector<wstring>& args)
{
	wchar_t cmd[1000];
	vector<wstring>::iterator i;
	wcscpy(cmd, L"");
	for(i=(++args.begin()); i!=args.end(); i++)
	{
		wcscat(cmd, (*i).c_str());
		wcscat(cmd, L" ");
	}
	ShellExecuteW(NULL, NULL, cmd, NULL, NULL, SW_HIDE);
	NetSendLine(L"done.");
}

void Cmd_exec(vector<wstring>& args)
{
	if(args.size()<2)
	{
		NetSendLine(L"invalid parameter");
		return;
	}

	wchar_t param[1000];
	param[0] = L'\0';
	
	for(size_t i=2; i<args.size(); i++)
	{
		wcscat(param, args[i].c_str());
		wcscat(param, L" ");
	}

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if(!CreateProcessW(args[1].c_str(), param, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}

	NetSendLine(L"done");
}

void Cmd_close(vector<wstring>& args)
{
	if(g_hClient!=INVALID_SOCKET)
	{
		shutdown(g_hClient, SD_BOTH);
		closesocket(g_hClient);
		g_hClient = INVALID_SOCKET;
	}
}

void Cmd_kill(vector<wstring>& args)
{
	if(args.size()!=2)
	{
		NetSendLine(L"invalid parameter");
		return;
	}

	wchar_t val[100];
	wsprintf(val, L"%d", _wtoi(args[1].c_str()));
	if(wcscmp(val, args[1].c_str())==0)
	{
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)_wtoi(val));
		if(hProcess)
		{
			TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
		}
	}
	else
	{
		DWORD dwPIDs[1000];
		DWORD dwSize;
		if(EnumProcesses(dwPIDs, sizeof(dwPIDs), &dwSize))
		{
			for(DWORD i=0; i<dwSize/sizeof(DWORD); i++)
			{
				HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPIDs[i]);
				if(hProcess)
				{
					wchar_t file[MAX_PATH];
					if(GetModuleFileNameExW(hProcess, NULL, file, sizeof(file))>0)
					{
						wchar_t* name = wcsrchr(file, L'\\');
						if(name && wcsicmp(name+1, args[1].c_str())==0)
						{
							TerminateProcess(hProcess, 0);
						}
					}
					CloseHandle(hProcess);
				}
			}
		}
	}

	NetSendLine(L"done");
}
