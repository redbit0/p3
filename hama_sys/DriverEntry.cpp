/*-----------------------------------------------------------------------------
* DriverEntry.cpp
*-----------------------------------------------------------------------------
*
*-----------------------------------------------------------------------------
* All rights reserved by somma (fixbrain@gmail.com, unsorted@msn.com)
*-----------------------------------------------------------------------------
* - 10.11.2010 created
**---------------------------------------------------------------------------*/
#include "DriverHeaders.h"
#include "DriverDebug.h"
#include "fc_drv_util.h"

#include "start_vm.h"


NTSTATUS MajorDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS MajorCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS MajorClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
VOID DriverUnload(IN PDRIVER_OBJECT DriverObject);
PDEVICE_OBJECT	pDeviceObject = NULL;
UNICODE_STRING	uniDriverName;
UNICODE_STRING	uniDosDriverName;
int pid = -1;
int cr3_val = -1;

#define MY_IOCTL_INDEX 0x800
#define IOCTL_MY_FUNCTION1 CTL_CODE(FILE_DEVICE_UNKNOWN, MY_IOCTL_INDEX, METHOD_BUFFERED, FILE_ANY_ACCESS)


/**----------------------------------------------------------------------------
\brief  DriverEntry

\param
\return
\code
\endcode
-----------------------------------------------------------------------------*/
extern "C"
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS            ntStatus;


	log_info
		"\n\n===============================================================================\n"\
		"driver Compiled at %s on %s \n"\
		"===============================================================================\n",
		__TIME__, __DATE__
		log_end


		start_vm();


	//> device 이름 생성
	RtlInitUnicodeString(&uniDriverName, L"\\Device\\matrix");
	RtlInitUnicodeString(&uniDosDriverName, L"\\DosDevices\\matrix");

	ntStatus = IoCreateDevice(DriverObject, 0, &uniDriverName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, TRUE, &pDeviceObject);
	if (FALSE == NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}

	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MajorDeviceControl;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = MajorCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = MajorClose;

	DriverObject->DriverUnload = DriverUnload;

	ntStatus = IoCreateSymbolicLink(&uniDosDriverName, &uniDriverName);
	if (!NT_SUCCESS(ntStatus)) {
		IoDeleteDevice(DriverObject->DeviceObject);
		return ntStatus;
	}

	return STATUS_SUCCESS;
}

VOID DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT pDeviceObject;

	pDeviceObject = DriverObject->DeviceObject;
	IoDeleteSymbolicLink(&uniDosDriverName);
	IoDeleteDevice(DriverObject->DeviceObject);

	DbgPrint("Driver Unload! \n");
}


NTSTATUS MajorDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PIO_STACK_LOCATION IrpStack;
	ULONG dwFunctionCode;
	ULONG *Buffer;
	ULONG base = 0;
	ULONG old_base = -1;
	ULONG pid_tmp = -1;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);
	dwFunctionCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;
	Buffer = (ULONG*)Irp->AssociatedIrp.SystemBuffer;

	DbgPrint("In here\n");
	switch (dwFunctionCode) {
	case 0x800:
		pid = *Buffer;


		__asm{
			mov eax, fs:0x124
				mov eax, [eax + 0x44]
				mov old_base, eax
		};
		base = old_base;
		while (1){
			__asm{
				mov eax, base
					mov eax, [eax + 0x84]
					mov pid_tmp, eax
			};
			if (pid_tmp == pid){
				__asm{
					mov eax, base
						mov eax, [eax + 0x18]
						mov cr3_val, eax
				};
				break;
			}
			else{
				__asm{
					mov eax, base
						mov eax, [eax + 0x88]
						mov eax, [eax]
						sub eax, 0x88
						mov base, eax
				}
			}
			if (base == old_base){
				DbgPrint("no process\n");
				break;
			}
		}
		DbgPrint("PID : %d\n", pid);
		DbgPrint("CR3 : %x\n", cr3_val);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = sizeof(ULONG);
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS MajorCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS MajorClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}