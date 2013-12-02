/******************************************************************************
 * hama_app.cpp
 ******************************************************************************
 * 
 ******************************************************************************
 * All rights reserved by somma (fixbrain@gmail.com)
 ******************************************************************************
 * 2013/10/15   22:14 created
******************************************************************************/


#include <Windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include "resource.h"
#include "scm_context.h"
#include "util.h"
#include <iostream>

static pscm_context _scm_context = NULL;

static const wchar_t* _hama_driver_name = L"hama.sys";
static const wchar_t* _hama_service_name = L"hama";
static const wchar_t* _hama_service_display = L"BoBoB";
TCHAR *deviceSymName = TEXT("\\\\.\\matrix");

#define DEVICE_IO_CONTORL_SEND_PID 0x800

HINSTANCE hInstance;


HANDLE hDriver;
BOOL loaded_driver;

int hyper_animation;
int screen_x, screen_y;
LRESULT __stdcall draw_proc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	switch (iMessage){
		case WM_CREATE:
			screen_x = GetSystemMetrics(SM_CXSCREEN);
			screen_y = GetSystemMetrics(SM_CYSCREEN);
			SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 100, LWA_COLORKEY);
			SetTimer(hWnd, 0, 20, NULL);
			return 0;

		case WM_PAINT:{
						  PAINTSTRUCT ps;
						  HDC hdc = BeginPaint(hWnd, &ps);
						  
						  SelectObject(hdc, GetStockObject(WHITE_PEN));
						  SelectObject(hdc, GetStockObject(WHITE_BRUSH));
						  Rectangle(hdc, 0, 0, screen_x, screen_y);

						  HPEN red_brush = CreatePen(PS_SOLID, 50, RGB(255, 0, 0));//CreateSolidBrush(RGB(255, 0, 0));
						  HPEN old_brush = (HPEN)SelectObject(hdc, red_brush);

						  MoveToEx(hdc, 0, hyper_animation, NULL);
						  LineTo(hdc, screen_x, hyper_animation);

						  DeleteObject(SelectObject(hdc, old_brush));
						  EndPaint(hWnd, &ps);
						  break;
		}
		case WM_TIMER:
			if (hyper_animation > screen_y){
				KillTimer(hWnd, 0);
				ShowWindow(hWnd, SW_HIDE);
			}
			hyper_animation += 25;
			InvalidateRect(hWnd, NULL, TRUE);
			break;
	}

	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}
void animate()
{
	HDC hdc = GetWindowDC(NULL);
	int x = screen_x  = GetSystemMetrics(SM_CXSCREEN);
	int y = screen_y = GetSystemMetrics(SM_CYSCREEN);

	WNDCLASS wndClass;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hbrBackground = NULL;// (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndClass.hCursor = NULL;
	wndClass.hIcon = NULL;
	wndClass.hInstance = hInstance;
	wndClass.lpfnWndProc = draw_proc;
	wndClass.lpszClassName = TEXT("draw_window");
	wndClass.lpszMenuName = NULL;
	wndClass.style = NULL;

	RegisterClass(&wndClass);
	CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_COMPOSITED | WS_EX_TOOLWINDOW, TEXT("draw_window"), TEXT("draw_window"), WS_POPUP | WS_VISIBLE, 0, 0, x, y, NULL, NULL, hInstance, NULL);

}

int init_all(HWND hWnd)
{
	loaded_driver = FALSE;
	HBITMAP hBitmap_bg = (HBITMAP)LoadImage(NULL, L"./resources/bg.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	HBITMAP hBitmap_refresh = (HBITMAP)LoadImage(NULL, L"./resources/refresh.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	HBITMAP hBitmap_load = (HBITMAP)LoadImage(NULL, L"./resources/load.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	HBITMAP hBitmap_exit = (HBITMAP)LoadImage(NULL, L"./resources/exit.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);


	SendDlgItemMessage(hWnd, IDC_BG, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap_bg);
	SendDlgItemMessage(hWnd, IDC_BUTTON_REFRESH, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap_refresh);
	SendDlgItemMessage(hWnd, IDOK, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap_load);
	SendDlgItemMessage(hWnd, IDCANCEL, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap_exit);

	return 1;
}

int load_driver(HWND hWnd)
{
	animate();
	ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_REFRESH), SW_SHOW);
	SetDlgItemText(hWnd, IDOK, TEXT("UNLOAD"));

	_ASSERTE(NULL == _scm_context);
	if (NULL != _scm_context) return true;

	std::wstring module_dir;
	if (true != get_current_module_dir(module_dir))
	{
		log_err L"get_current_module_dir()" log_end
		return -1;
	}

	std::wstring driver_path(module_dir);
	driver_path += L"\\";
	driver_path += _hama_driver_name;

	if (true != is_file_existsW(driver_path.c_str()))
	{
		log_err L"no driver file = %s", driver_path.c_str() log_end
		return -1;
	}

	_scm_context = new scm_context(
		driver_path.c_str(),
		_hama_service_name,
		_hama_service_display,
		true
		);

	if (NULL == _scm_context)
	{
		log_err L"insufficient resources for allocate scm_context()" log_end
			return -1;
	}

	if (true != _scm_context->install_driver())
	{
		log_err L"scm_context::install_driver()" log_end
		delete _scm_context; _scm_context = NULL;
		return -1;
	}

	if (true != _scm_context->start_driver())
	{
		log_err L"scm_context::start_driver()" log_end
		return -1;
	}

	log_info L"%s service started successfully", _hama_service_name log_end


	hDriver = CreateFile(deviceSymName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	loaded_driver = TRUE;
	return 1;
}

int unload_driver(HWND hWnd)
{
	ShowWindow(GetDlgItem(hWnd, IDC_BUTTON_REFRESH), SW_HIDE);
	SetDlgItemText(hWnd, IDOK, TEXT("LOAD"));

	if (NULL == _scm_context) return true;			// just ignore this situation

	_scm_context->start_driver();
	_scm_context->uninstall_driver();
	log_info L"%s service stopped successfully", _hama_service_name log_end

	loaded_driver = FALSE;
	return 1;
}

int refresh(HWND hWnd)
{
	PROCESSENTRY32 ppe;
	TCHAR buf[512];
	HANDLE hSnapshot;

	SendDlgItemMessage(hWnd, IDC_LIST, LB_RESETCONTENT, 0, 0);
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	ppe.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(hSnapshot, &ppe))
	do{
		_stprintf_s(buf, 260, TEXT("%s <%-5d>"), ppe.szExeFile, ppe.th32ProcessID);
		SendDlgItemMessage(hWnd, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)buf);
	} while (Process32Next(hSnapshot, &ppe));
	return 1;
}

int list_enter(HWND hWnd)
{
	int idx = SendDlgItemMessage(hWnd, IDC_LIST, LB_GETCURSEL, 0, 0);
	TCHAR info[512];
	SendDlgItemMessage(hWnd, IDC_LIST, LB_GETTEXT, idx, (LPARAM)&info);

	if (lstrlen(info) > 0)
	{
		int i = 0, b;
		while (info[i++] != '<');
		b = i;
		while (info[b++] != '>');
		info[b - 1] = '\0';

		DWORD pid = _tstoi(&info[i]);

		if (!DeviceIoControl(hDriver, DEVICE_IO_CONTORL_SEND_PID, &pid, 4, NULL, 0, /*nothing*/(DWORD*)&pid, NULL))
		{
			MessageBox(hWnd, TEXT("CANT SEND PID"), TEXT("ERROR"), MB_OK | MB_ICONERROR);
		}
		else{
			MessageBox(hWnd, TEXT("SEND PID"), TEXT("SUCCESS"), MB_OK | MB_ICONINFORMATION);
		}
	}

	return 0;
}


int __stdcall MainDialog(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	switch (iMessage){
	case WM_INITDIALOG:
		return init_all(hWnd);
	case WM_COMMAND:
		switch (LOWORD(wParam)){
		case IDOK:
			if (!loaded_driver)
				return load_driver(hWnd);
			else
				return unload_driver(hWnd);
		case IDCANCEL:
			return EndDialog(hWnd, 0);
		case IDC_BUTTON_REFRESH:
			return refresh(hWnd);
		case IDC_LIST:
			switch (HIWORD(wParam)){
			case LBN_DBLCLK:
				return list_enter(hWnd);
			}
		}
	}


	return 0;
}

int __stdcall WinMain(HINSTANCE _hInstance, HINSTANCE hPrevInstance, char* cmd, int show)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(cmd);
	UNREFERENCED_PARAMETER(show);
	hInstance = _hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG_MAIN), NULL, MainDialog);
	return 0;
}
