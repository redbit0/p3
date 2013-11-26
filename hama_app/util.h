/******************************************************************************
 * util.h
 ******************************************************************************
 * 
 ******************************************************************************
 * All rights reserved by somma (fixbrain@gmail.com)
 ******************************************************************************
 * 2013/10/15   22:18 created
******************************************************************************/
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sal.h>
#include <ostream>
#include <sstream>
#include <strsafe.h>

//> console logging stuffs
#define LL_DEBG		0
#define LL_INFO		1
#define LL_ERRR		2
#define LL_NONE		3

#define con_err		write_to_console( LL_ERRR, __FUNCTIONW__, 
#define con_info	write_to_console( LL_INFO, __FUNCTIONW__, 
#define con_dbg		write_to_console( LL_DEBG, __FUNCTIONW__, 
#define con_msg		write_to_console( LL_NONE, __FUNCTIONW__, 
#define con_end		);

#ifndef _slogger_included
	//> slogger 를 사용하지 않는 경우
	#define log_err		con_err
	#define log_dbg		con_dbg
	#define log_info	con_info
	#define	log_msg		con_msg
	#define log_end		con_end
	
#endif //_slogger_included

void write_to_console(_In_ DWORD log_level, _In_ wchar_t* function, _In_ wchar_t* format, ...);

bool get_current_module_path(_Out_ std::wstring& module_path);
bool get_current_module_dir(_Out_ std::wstring& module_dir);
bool extract_last_tokenW(_In_ std::wstring& org_string, _In_ const std::wstring& token, _Out_ std::wstring& out_string, _In_ bool forward,_In_ bool delete_token);
bool is_file_existsW(_In_ const wchar_t* file_path);