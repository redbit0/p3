/******************************************************************************
 * scm_context.cpp
 ******************************************************************************
 * 
 ******************************************************************************
 * All rights reserved by somma (fixbrain@gmail.com)
 ******************************************************************************
 * 2011/11/30   created
******************************************************************************/

#include "scm_context.h"
#include <memory>				// std::shared_ptr
#include "util.h"

//> todo - SCM 접근시 권한은 필요한 만큼만 정해서 호출하게 하자. 

struct sc_handle_deleter
{
	void operator()(SC_HANDLE* phandle) const
	{
		CloseServiceHandle(*phandle);
		delete phandle;
	}
};

typedef std::shared_ptr<SC_HANDLE> sc_handle_ptr;


/**
* @brief	
* @param	
* @see		
* @remarks	
* @code		
* @endcode	
* @return	
*/
scm_context::scm_context(
	_In_z_ const wchar_t* driver_path, 
	_In_z_ const wchar_t* service_name, 
	_In_z_ const wchar_t* service_display_name,
	_In_ bool uninstall_service_on_free_context
	)
:	_uninstall_service_on_free(uninstall_service_on_free_context),
	_driver_handle(INVALID_HANDLE_VALUE), 
	_driver_path(driver_path),
	_service_name(service_name),
	_service_display_name(service_display_name),
	_installed(false), 
	_running(false)
{
}

/**
* @brief	
* @param	
* @see		
* @remarks	
* @code		
* @endcode	
* @return	
*/
scm_context::~scm_context()
{
	if(INVALID_HANDLE_VALUE != _driver_handle) 
	{
		CloseHandle(_driver_handle); _driver_handle = INVALID_HANDLE_VALUE;
	}
	stop_driver();
	
	if (true == _uninstall_service_on_free)
	{
		uninstall_driver();
	}
}

/**
* @brief	
* @param	
* @see		
* @remarks	
* @code		
* @endcode	
* @return	
*/
bool scm_context::install_driver()
{
	SC_HANDLE scm_handle = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == scm_handle)
	{
		log_err L"OpenSCManagerW() faield. gle=0x%08x", GetLastError() log_end
		return false;
	}
	sc_handle_ptr scm_handle_ptr(new SC_HANDLE(scm_handle), sc_handle_deleter());

	// already exists?
	SC_HANDLE service_handle = OpenServiceW(scm_handle, _service_name.c_str(), SERVICE_QUERY_CONFIG);
	if (NULL != service_handle)
	{
		log_dbg L"service=%ws. already exists", _service_name.c_str() log_end
		CloseServiceHandle(service_handle);
	}
	else
	{
		// Install the driver
		service_handle = CreateServiceW(
									scm_handle,
									_service_name.c_str(),
									_service_display_name.c_str(),
									GENERIC_READ, // SERVICE_ALL_ACCESS,
									SERVICE_KERNEL_DRIVER,
									SERVICE_DEMAND_START,
									SERVICE_ERROR_NORMAL,
									_driver_path.c_str(),
									NULL,
									NULL,
									NULL,
									NULL,
									NULL
									);
		if (service_handle == NULL)
		{	
			log_err 
				L"CreateServcieW(path=%ws, svc_name=%ws, svc_display=%ws) failed. gle=0x%08x",
				_driver_path.c_str(), _service_name.c_str(), _service_display_name.c_str(), GetLastError()
			log_end
			return false;
		}
		CloseServiceHandle(service_handle);
		log_info L"service=%ws installed successfully", _service_name.c_str() log_end
	}

	_installed = true;
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
*/
bool scm_context::uninstall_driver()
{
	if (true != _installed) return true;

	if (true == _running)
	{
		if(true != stop_driver())
		{
			log_err L"scm_context::stop_driver() failed, can not uninstall driver..." log_end
			
			//> stop_driver() 가 실패해도 삭제시도를 해야 한다. 
			//    - driver :: DriverEntry() 에서 STATUS_SUCCESS 를 리턴했으나 아무짓도 안하고, 리턴한 경우
			//    - driver handle 을 누군가 물고 있는 경우
			//  강제로 서비스를 삭제 (registry 에서 서비스 제거)하고, 리부팅하면 서비스가 
			//  제거된 상태로 (정상) 돌아올 수 있다. 
			//
			//return false;

			_running = false;
		}
	}

	SC_HANDLE scm_handle = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == scm_handle)
	{
		log_err L"OpenSCManagerW() faield. gle=0x%08x", GetLastError() log_end
		return false;
	}
	sc_handle_ptr scm_handle_ptr(new SC_HANDLE(scm_handle), sc_handle_deleter());

	SC_HANDLE service_handle = OpenServiceW(
									scm_handle,
									_service_name.c_str(), 
									SERVICE_ALL_ACCESS
									);
	if (NULL == service_handle)
	{
		log_err 
			L"OpenServiceW( service_name=%ws ) failed. gle=0x%08x", 
			_service_name.c_str(), GetLastError() 
		log_end
		return false;
	}
	sc_handle_ptr service_handle_ptr(new SC_HANDLE(service_handle), sc_handle_deleter());

	if (FALSE == DeleteService(service_handle))
	{
		DWORD err = GetLastError();
		if (ERROR_SERVICE_MARKED_FOR_DELETE != err)
		{
			log_err 
				L"DeleteService( service name=%ws ) failed, gle=0x%08x", 
				_service_name.c_str(), err
			log_end
			return false;
		}
	}

	_installed = false;
	log_info L"service=%ws deleted successfully", _service_name.c_str() log_end
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
*/
bool scm_context::start_driver()
{
	_ASSERTE(true == _installed);
	if (true != _installed) return false;
	if (true == _running) return true;

	SC_HANDLE scm_handle = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == scm_handle)
	{
		log_err L"OpenSCManagerW() faield. gle=0x%08x", GetLastError() log_end
		return false;
	}
	sc_handle_ptr scm_handle_ptr(new SC_HANDLE(scm_handle), sc_handle_deleter());

	SC_HANDLE service_handle = OpenServiceW(
									scm_handle,
									_service_name.c_str(), 
									SERVICE_ALL_ACCESS
									);
	if (NULL == service_handle)
	{
		log_err 
			L"OpenServiceW( service_name=%ws ) failed. gle=0x%08x", 
			_service_name.c_str(), GetLastError() 
		log_end
		return false;
	}
	sc_handle_ptr service_handle_ptr(new SC_HANDLE(service_handle), sc_handle_deleter());

	if (TRUE != StartService(service_handle, 0, NULL))
	{
		DWORD err = GetLastError();
		if (err != ERROR_SERVICE_ALREADY_RUNNING)
		{
            log_err 
				L"StartService( service name=%ws ) failed, gle=0x%08x", 
				_service_name.c_str(), err
			log_end
			return false;
		}
	}

	_running = true;
	log_info L"service=%ws started successfully", _service_name.c_str() log_end
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
*/
bool scm_context::stop_driver()
{
	if (true != _installed) return false;
	if (true != _running) return true;

	SC_HANDLE scm_handle = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == scm_handle)
	{
		log_err L"OpenSCManagerW() faield. gle=0x%08x", GetLastError() log_end
		return false;
	}
	sc_handle_ptr scm_handle_ptr(new SC_HANDLE(scm_handle), sc_handle_deleter());

	SC_HANDLE service_handle = OpenServiceW(
									scm_handle,
									_service_name.c_str(), 
									SERVICE_ALL_ACCESS
									);
	if (NULL == service_handle)
	{
		log_err 
			L"OpenServiceW( service_name=%ws ) failed. gle=0x%08x", 
			_service_name.c_str(), GetLastError() 
		log_end
		return false;
	}
	sc_handle_ptr service_handle_ptr(new SC_HANDLE(service_handle), sc_handle_deleter());

	// 2007.05.17 by somma
	// 다른 프로세스가 SCM 을 통해서 SERVICE_CONTROL_STOP 을 이미요청한 경우
	// 여기서 호출한 ControlService() 함수는 FALSE 를 리턴한다.
	// 그러나 서비스는 정상 종료된다.
	//
	SERVICE_STATUS service_status={0};
	if (FALSE == ControlService(service_handle, SERVICE_CONTROL_STOP, &service_status))
	{
		log_err
			L"ControlService( service name=%ws ) failed, gle=0x%08x", 
			_service_name.c_str(), GetLastError()
		log_end
		return false;
	}

	_running = false;
	log_info L"service=%ws stopped successfully", _service_name.c_str() log_end
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
*/
bool 
scm_context::send_command(
		_In_ uint32_t io_code, 
		_In_ uint32_t input_buffer_size,
		_In_bytecount_(input_buffer_size) void* input_buffer,
		_In_ uint32_t output_buffer_size,
		_In_bytecount_(output_buffer_size) void* output_buffer,
		_Out_ uint32_t* bytes_returned
		)
{
	_ASSERTE(true == _installed);
	_ASSERTE(true == _running);
	if (true != _installed || true != _running) return false;

	if (INVALID_HANDLE_VALUE == _driver_handle)
	{
		_driver_handle = open_driver();
		if (INVALID_HANDLE_VALUE == _driver_handle)
		{
			log_err L"scm_context::open_driver() failed" log_end
			return false;
		}
	}

	BOOL ret = DeviceIoControl(
					_driver_handle, 
					io_code, 
					input_buffer, 
					input_buffer_size, 
					output_buffer, 
					output_buffer_size, 
					reinterpret_cast<LPDWORD>(bytes_returned),
					NULL
					);
	if(TRUE != ret)
	{
		log_err L"DeviceIoControl( io_code=0x%08x ) failed", io_code log_end
		return false;
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
*/
HANDLE scm_context::open_driver()
{
	_ASSERTE(true == _installed);
	_ASSERTE(true == _running);
	_ASSERTE(INVALID_HANDLE_VALUE == _driver_handle);
	if (true != _installed || true != _running) return INVALID_HANDLE_VALUE;
	if (INVALID_HANDLE_VALUE != _driver_handle) return _driver_handle;

	std::wstring driver_object_name= L"\\\\.\\" + _service_name;
	HANDLE driver_handle = CreateFileW(
							driver_object_name.c_str(),
							GENERIC_READ | GENERIC_WRITE, 
							0, // exclusive open
							NULL, 
							OPEN_EXISTING, 
							FILE_ATTRIBUTE_NORMAL, 
							0
							);
	if (INVALID_HANDLE_VALUE == driver_handle)
	{
		log_err
			L"CreateFileW(driver name=%ws) failed, gle=0x%08x", 
			driver_object_name.c_str(), GetLastError()
		log_end
		return INVALID_HANDLE_VALUE;
	}

	return driver_handle;
}