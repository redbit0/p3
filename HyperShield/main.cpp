#include <Windows.h>
#include <tchar.h>
#include <TlHelp32.h>
#include <Mmsystem.h>

#pragma comment(lib,"winmm.lib")

HINSTANCE hInst;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
void Snapshot();

HWND hList;
HWND ghWnd;
HANDLE hMyDriver;
HBITMAP hBitmap;

TCHAR *DeviceName = L"\\\\.\\matrix";

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	MSG msg;

	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"HyperShield";
	wcex.hIconSm = NULL;

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;

	ghWnd = CreateWindow(L"HyperShield", L"HyperShield", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 429, 541, NULL, NULL, hInstance, NULL);

	if (!ghWnd)
	{
		return FALSE;
	}

	ShowWindow(ghWnd, nCmdShow);
	UpdateWindow(ghWnd);

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_CREATE:
		PlaySound(L"./resources/bgm.wav", NULL, SND_ASYNC | SND_LOOP);
		hMyDriver = CreateFile(DeviceName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		hList = CreateWindowEx(NULL, TEXT("LISTBOX"), L"", WS_CHILD | WS_VISIBLE | 1 | WS_VSCROLL | ES_AUTOVSCROLL, 95, 120, 220, 250, hWnd, NULL, hInst, NULL);
		hBitmap = (HBITMAP)LoadImage(NULL, L"./resources/bg.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		Snapshot();
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		switch (wmEvent)
		{
		case LBN_DBLCLK:
			if (wmId == 0)
			{
				TCHAR Text_List[256];

				int Index;
				int Size_Text;

				Index = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
				SendMessage((HWND)lParam, LB_GETTEXT, Index, (LPARAM)&Text_List);

				if (Size_Text = lstrlen(Text_List) > 0)
				{
					TCHAR* p;
					int pid;
#pragma warning(push)
#pragma warning(disable : 4996)
					p = wcstok(Text_List, L" ");
					p = wcstok(NULL, p);
#pragma warning(push)
					pid = _wtoi(p);
					if (!DeviceIoControl(hMyDriver, 0x800, &pid, 4, NULL, 0, NULL, NULL))
					{
						MessageBox(0, L"전송 실패", p, 0);
					}
				}

			}

			break;
		}
	case WM_PAINT:{
					  hdc = BeginPaint(hWnd, &ps);
					  HDC hMemDC = CreateCompatibleDC(hdc);
					  HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
					  BitBlt(hdc, 0, 0, 413, 502, hMemDC, 0, 0, SRCCOPY);
					  SelectObject(hMemDC, hOldBitmap);
					  DeleteObject(hBitmap);
					  DeleteDC(hMemDC);
					  EndPaint(hWnd, &ps);
					  break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void Snapshot()
{
	PROCESSENTRY32 ppe;

	TCHAR buf[260] = L"";

	BOOL Get = FALSE;

	HANDLE hSnapshot;

	DWORD dwPID = 0xFFFFFFFF;

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // 프로세스 목록 얻기 위한 스냅샷 NULL 부분은 Proc ID를 줄 수 있다. 입력받으면 넘겨줘야함
	ppe.dwSize = sizeof(PROCESSENTRY32);
	Get = Process32First(hSnapshot, &ppe);

	while (Get)
	{
		if (dwPID = ppe.th32ProcessID)
		{
			wsprintf(buf, L"%ws %04d", ppe.szExeFile, dwPID);
			SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)buf);
		}
		Get = Process32Next(hSnapshot, &ppe);
	}

	Process32First(hSnapshot, &ppe);
}