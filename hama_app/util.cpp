/******************************************************************************
 * util.cpp
 ******************************************************************************
 * 
 ******************************************************************************
 * All rights reserved by somma (fixbrain@gmail.com)
 ******************************************************************************
 * 2013/10/15   22:21 created
******************************************************************************/

#include "util.h"


/**
 * @brief	
 * @param	
 * @see		
 * @remarks	
 * @code		
 * @endcode	
 * @return	
**/
void write_to_console(_In_ DWORD log_level, _In_ wchar_t* function, _In_ wchar_t* format, ...)
{
	static HANDLE	_stdout_handle = INVALID_HANDLE_VALUE;
	static WORD		_old_color = 0x0000;
	
	//> initialization for console text color manipulation.
	if (INVALID_HANDLE_VALUE == _stdout_handle)
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;

		_stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
		GetConsoleScreenBufferInfo(_stdout_handle, &csbi);
		_old_color = csbi.wAttributes;
	}

	std::wstringstream strm_prefix;
	switch (log_level)
	{
	case LL_DEBG: strm_prefix << L"[DEBG] " << function << L", "; break;
	case LL_INFO: strm_prefix << L"[INFO] " << function << L", "; break;
	case LL_ERRR: strm_prefix << L"[ERR ] " << function << L", "; break;		
	default:
		// LL_NONE
		break;		
	}

	DWORD len;
	va_list args;
	WCHAR msg[4096];	
	
	va_start(args, format);
	if(TRUE != SUCCEEDED(StringCchVPrintf(msg, sizeof(msg), format, args))){ return; }
	va_end(args);

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_INTENSITY);	
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), strm_prefix.str().c_str(), (DWORD)strm_prefix.str().size(), &len, NULL);
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, (DWORD)wcslen(msg), &len, NULL);	
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), L"\n", (DWORD)wcslen(L"\n"), &len, NULL);
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), _old_color);	
}



/**
 * @brief	현재 모듈의 full path 를 구한다. 
 * @param	
 * @see		
 * @remarks	
 * @code		
 * @endcode	
 * @return	
**/
bool get_current_module_path(_Out_ std::wstring& module_path)
{
	DWORD  ret = 0;
	DWORD  buf_len = MAX_PATH;
	LPTSTR buf = (LPTSTR) malloc( buf_len * sizeof(TCHAR) );
	if (NULL == buf) return false;
	
	for(;;)
	{
		ret = GetModuleFileName(GetModuleHandle(NULL), buf, buf_len);
		if (ret == buf_len)
		{
			// buf 가 작은 경우 buf_len 만큼 버퍼가 잘리고, buf_len 리턴 (에러로 간주)
			// 버퍼 사이즈를 2배 늘려서 재 시도
			free(buf);

			buf_len *= 2;
			buf = (LPTSTR) malloc( buf_len * sizeof(TCHAR) );
			if (NULL == buf) return false;
		}
		else
		{
			module_path = buf;
			free(buf);
			return true;
		}
	}
	//return false;	// never reach here!
}

/**
 * @brief	
 * @param	
 * @see		
 * @remarks	
 * @code		
 * @endcode	
 * @return	
**/
bool get_current_module_dir(_Out_ std::wstring& module_dir)
{
	std::wstring module_path;
	if (true != get_current_module_path(module_path))
	{
		log_err L"get_current_module_path()" log_end
		return false;
	}
	
	if (true != extract_last_tokenW(module_path, L"\\", module_dir, true, false))
	{
		log_err L"extract_last_tokenW( org=%s )", module_path.c_str() log_end
		module_dir = L"";
		return false;
	}

	return true;
}

/**
 * @brief	org_string 에서 token 을 검색해서 문자열을 잘라낸다. 
			(org_string 의 뒤에서부터 token 을 검색)

			ABCDEFG.HIJ.KLMN	: org_string
					   .		: token
			ABCDEFG.HIJ			: out_string if forward = TRUE
					    KLMN	: out_string if forward = FALSE

			delete_token 가 True 인 경우 org_string 에서 out_string + token 을 삭제
 * @param	
 * @see		
 * @remarks	
 * @code		
 * @endcode	
 * @return	
**/
bool 
extract_last_tokenW(
	_In_ std::wstring& org_string,
	_In_ const std::wstring& token,
	_Out_ std::wstring& out_string, 
	_In_ bool forward,
	_In_ bool delete_token
	)
{
    if (true== delete_token)
    {
        if (&org_string == &out_string) 
        {
#ifndef TEST_EXPORTS
            _ASSERTE(!"prarameters conflict! ");
#endif
            return false;
        }
    }

    size_t pos = org_string.rfind(token);
	if (std::wstring::npos == pos)
	{
        out_string = org_string;
        return true;
	}

	if (true== forward)
	{
		out_string = org_string.substr(0, pos);
        if (delete_token) org_string.erase(0, pos + token.size());
	}
	else
	{
        out_string = org_string.substr(pos + token.size(), org_string.size());
        if (delete_token) org_string.erase(pos, org_string.size());
	}
    return true;
}

/**
 * @brief	
 * @param	
 * @see		
 * @remarks	
 * @code		
 * @endcode	
 * @return	
**/
bool is_file_existsW(_In_ const wchar_t* file_path)
{
	_ASSERTE(NULL != file_path);
	_ASSERTE(TRUE != IsBadStringPtrW(file_path, MAX_PATH));
	if ((NULL == file_path) || (TRUE == IsBadStringPtrW(file_path, MAX_PATH))) return false;

	WIN32_FILE_ATTRIBUTE_DATA info = {0};

	//
	// CreateFile()이 아닌 GetFileAttributesEx()를 이용하면 파일이 다른 process에 의해 lock되어 있어도
	// 파일 존재여부를 정확히 체크할 수 있다.
	//
	if (GetFileAttributesExW(file_path, GetFileExInfoStandard, &info)==0) 
		return false;
	else
		return true;
}
