/**----------------------------------------------------------------------------
* start_vm.cpp
*-----------------------------------------------------------------------------
*
*-----------------------------------------------------------------------------
* All rights reserved by Noh,Yonghwan (fixbrain@gmail.com, unsorted@msn.com)
*-----------------------------------------------------------------------------
* 18:10:2013   20:31 created
**---------------------------------------------------------------------------*/
#include "DriverHeaders.h"
#include "DriverDebug.h"
#include "scancode.h"
#include "start_vm.h"





///////////////
//           //
//  Globals  //
//           //
///////////////
ULONGLONG temp64;


VCPU	_vcpu = { 0 };

ULONG			ErrorCode = 0;

EFLAGS			eFlags = { 0 };
MSR				msr = { 0 };

ULONG			HandlerLogging = 0;
ULONG			ScrubTheLaunch = 0;

VMX_FEATURES				vmxFeatures;
DWORD						vmxFeatures2;
IA32_VMX_BASIC_MSR			vmxBasicMsr;
IA32_FEATURE_CONTROL_MSR	vmxFeatureControl;

CR0_REG						cr0_reg = { 0 };
CR4_REG						cr4_reg = { 0 };

ULONG						temp32 = 0;
USHORT						temp16 = 0;

GDTR						gdt_reg = { 0 };
IDTR						idt_reg = { 0 };

ULONG						gdt_base = 0;
ULONG						idt_base = 0;

USHORT						mLDT = 0;
USHORT						seg_selector = 0;

SEG_DESCRIPTOR				segDescriptor = { 0 };
MISC_DATA					misc_data = { 0 };

PVOID						GuestReturn = NULL;
ULONG						GuestStack = 0;

MTRR_RANGE ranges[MAX_SUPPORTED_MTRR_RANGE];
MTRR_FIXED_RANGE fixed_ranges[MAX_SUPPORTED_MTRR_FIXED_RANGE];

unsigned char EPTGetMemoryType(unsigned __int32 address)
{
  unsigned char type = 0, index;
  if(address < 0x100000) { /* Check in fixed ranges */
    if(address < 0x80000) { /* 00000000:00080000 */
      type = (fixed_ranges[0].types & (0xff << ((address >> 16) * 8))) >> ((address >> 16) * 8);
    }
    else if (address < 0xa0000){
      address -= 0x80000;
      type = (fixed_ranges[1].types & (0xff << ((address >> 14) * 8))) >> ((address >> 14) * 8);
    }
    else if (address < 0xc0000) {
      address -= 0xa0000;
      type = (fixed_ranges[2].types & (0xff << ((address >> 14) * 8))) >> ((address >> 14) * 8);
    }
    else {
      index = ((address - 0xc0000) >> 15) + 3;
      address &= 0x7fff;
      type = (fixed_ranges[index].types & (0xff << ((address >> 12) * 8))) >> ((address >> 12) * 8);
    }
  }
  else {
    index = 0;
    while (index < MAX_SUPPORTED_MTRR_RANGE) {
      if(ranges[index].base <= address && (ranges[index].base+ranges[index].size) > address) {
        return ranges[index].type;
                        }
      index++;
    }
    type = 6;
  }
  return type;
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
VOID __stdcall VM_DispatchUnload(IN  PDRIVER_OBJECT  DriverObject)
{
	ULONG		ExitEFlags = 0;

	ULONG		ExitEAX = 0;
	ULONG		ExitECX = 0;
	ULONG		ExitEDX = 0;
	ULONG		ExitEBX = 0;
	ULONG		ExitESP = 0;
	ULONG		ExitEBP = 0;
	ULONG		ExitESI = 0;
	ULONG		ExitEDI = 0;

	DbgPrint("[vmm-unload] Active Processor Bitmap  [%08X]\n", (ULONG)KeQueryActiveProcessors());

	DbgPrint("[vmm-unload] Disabling VMX mode on CPU 0.\n");
	KeSetSystemAffinityThread((KAFFINITY)0x00000001);

	__asm
	{
		PUSHAD
			MOV		EAX, 0x12345678

			_emit 0x0F		// VMCALL
			_emit 0x01
			_emit 0xC1

			POPAD
	}

	DbgPrint("[vmm-unload] Freeing memory regions.\n");

	free_vcpu(_vcpu);

	DbgPrint("[vmm-unload] Driver Unloaded.\n");
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
NTSTATUS start_vm()
{
	NTSTATUS	status = STATUS_UNSUCCESSFUL;

	ULONG		EntryEFlags = 0;
	ULONG		cr4 = 0;

	ULONG		EntryEAX = 0;
	ULONG		EntryECX = 0;
	ULONG		EntryEDX = 0;
	ULONG		EntryEBX = 0;
	ULONG		EntryESP = 0;
	ULONG		EntryEBP = 0;
	ULONG		EntryESI = 0;
	ULONG		EntryEDI = 0;


	init_scancode();

	status = allocate_vcpu(_vcpu);
	if (!NT_SUCCESS(status))
	{
		log_err "allocate_vcpu(), status = 0x%08x", status log_end
			return status;
	}

	//	Save the state of the architecture.
	//
	__asm
	{
		CLI
			MOV		GuestStack, ESP

			PUSHAD
			POP		EntryEDI
			POP		EntryESI
			POP		EntryEBP
			POP		EntryESP
			POP		EntryEBX
			POP		EntryEDX
			POP		EntryECX
			POP		EntryEAX
			PUSHFD
			POP		EntryEFlags
	}

	StartVMX();

	//	Restore the state of the architecture.
	//
	__asm
	{
		PUSH	EntryEFlags
			POPFD
			PUSH	EntryEAX
			PUSH	EntryECX
			PUSH	EntryEDX
			PUSH	EntryEBX
			PUSH	EntryESP
			PUSH	EntryEBP
			PUSH	EntryESI
			PUSH	EntryEDI
			POPAD

			STI
			MOV		ESP, GuestStack
	}

	Log("Running on Processor", KeGetCurrentProcessorNumber());


	if (ScrubTheLaunch == 1)
	{
		Log("ERROR : Launch aborted.", 0);

		free_vcpu(_vcpu);
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		Log("VM is now executing.", 0);
	}

	return STATUS_SUCCESS;
}



///////////
//       //
//	VMX  //
//       //
///////////
__declspec(naked) VOID StartVMX()
{
	//
	//	Get the Guest Return EIP.
	//
	//
	//	Hi	|			|
	//		+-----------+
	//		|	 EIP	|
	//		+-----------+ <--	ESP after the CALL
	//	Lo	|			|
	//
	//

	__asm	POP	GuestReturn

	Log("Guest Return EIP", GuestReturn);

	///////////////////////////
	//                       //
	//	SET THREAD AFFINITY  //
	//                       //
	///////////////////////////
	Log("Enabling VMX mode on CPU 0", 0);
	KeSetSystemAffinityThread((KAFFINITY)0x00000001);
	Log("Running on Processor", KeGetCurrentProcessorNumber());

	////////////////
	//            //
	//  GDT Info  //
	//            //
	////////////////
	__asm
	{
		SGDT	gdt_reg
	}

	temp32 = 0;
	temp32 = gdt_reg.BaseHi;
	temp32 <<= 16;
	temp32 |= gdt_reg.BaseLo;
	gdt_base = temp32;
	Log("GDT Base", gdt_base);
	Log("GDT Limit", gdt_reg.Limit);

	////////////////////////////
	//                        //
	//  IDT Segment Selector  //
	//                        //
	////////////////////////////
	__asm	SIDT	idt_reg

	temp32 = 0;
	temp32 = idt_reg.BaseHi;
	temp32 <<= 16;
	temp32 |= idt_reg.BaseLo;
	idt_base = temp32;
	Log("IDT Base", idt_base);
	Log("IDT Limit", idt_reg.Limit);

	//	(1)	Check VMX support in processor using CPUID.
	__asm
	{
		PUSHAD

			MOV		EAX, 1
			CPUID

			// ECX contains the VMX_FEATURES FLAGS (VMX supported if bit 5 equals 1)
			MOV		vmxFeatures, ECX
			mov		vmxFeatures2, edx

			MOV		EAX, 0x80000008
			CPUID
			MOV		temp32, EAX

			POPAD
	}

	if (vmxFeatures.VMX == 0)
	{
		Log("VMX Support Not Present.", vmxFeatures);
		goto Abort;
	}

	Log("VMX Support Present.", vmxFeatures);

	if (vmxFeatures2 & (1 << 12))
	{
		Log("[MTRR_DEBUG] MTRR IS SUPPORTED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", vmxFeatures2);

		ReadMSR(0xfe); // IA32_MTRRCAP
		Log("[MTRR_DEBUG] number of MTRR Range resiger", msr.Lo & 0xff);
		if (msr.Lo & (1 << 8)) Log("[MTRR_DEBUG] Fix Range MTRR Support!!", msr.Lo);
		if (msr.Lo & (1 << 10)) Log("[MTRR_DEBUG] Write Combine Mem type support", msr.Lo);
		if (msr.Lo & (1 << 11)) Log("[MTRR_DEBUG] SMRR Interface support", msr.Lo);

		ReadMSR(0x2ff);///IA32_MTRR_DEF_TYPE
		if (msr.Lo & (1 << 11))
		{
			if (msr.Lo & (1 << 11)) Log("[MTRR_DEBUG] MTRR IS ENABLE!!!!! FTK", msr.Lo);
			if (msr.Lo & (1 << 10)) Log("[MTRR_DEBUG] MTRR FIX RANGE ENABLE!!!", msr.Lo);
			Log("[MTRR_DEBUG] MTRR DEFAULT MEMTYPE", msr.Lo & 0xff);
			/*
				if disable mtrr, default mem type is uc
			*/
		}
		{
#define PRINT_FIXRANGE_MTRR(msr_index, addr_size ) \
	ReadMSR(msr_index); \
	log_info "[MTRR_DEBUG] FIX RANGE %08x to %08x type : %d", addr, (addr + addr_size), (msr.Lo & 0xff) log_end addr += addr_size + 1;\
	log_info "[MTRR_DEBUG] FIX RANGE %08x to %08x type : %d", addr, (addr + addr_size), ((msr.Lo >> 8) & 0xff) log_end addr += addr_size + 1; \
	log_info "[MTRR_DEBUG] FIX RANGE %08x to %08x type : %d", addr, (addr + addr_size), ((msr.Lo >> 16) & 0xff) log_end addr += addr_size + 1; \
	log_info "[MTRR_DEBUG] FIX RANGE %08x to %08x type : %d", addr, (addr + addr_size), ((msr.Lo >> 24) & 0xff) log_end addr += addr_size + 1; \
	log_info "[MTRR_DEBUG] FIX RANGE %08x to %08x type : %d", addr, (addr + addr_size), (msr.Hi & 0xff) log_end addr += addr_size + 1; \
	log_info "[MTRR_DEBUG] FIX RANGE %08x to %08x type : %d", addr, (addr + addr_size), ((msr.Hi >> 8) & 0xff) log_end addr += addr_size + 1; \
	log_info "[MTRR_DEBUG] FIX RANGE %08x to %08x type : %d", addr, (addr + addr_size), ((msr.Hi >> 16) & 0xff) log_end addr += addr_size + 1; \
	log_info "[MTRR_DEBUG] FIX RANGE %08x to %08x type : %d", addr, (addr + addr_size), ((msr.Hi >> 24) & 0xff) log_end addr += addr_size + 1;
	

			DWORD addr = 0;
			PRINT_FIXRANGE_MTRR(0x250, 0xffff); // IA32_MTRR_FIX64K_00000
			PRINT_FIXRANGE_MTRR(0x258, 0x3fff); // IA32_MTRR_FIX16K_80000
			PRINT_FIXRANGE_MTRR(0x259, 0x3fff); // IA32_MTRR_FIX16K_A0000
			PRINT_FIXRANGE_MTRR(0x268, 0xfff); // IA32_MTRR_FIX4K_C0000
			PRINT_FIXRANGE_MTRR(0x269, 0xfff); // IA32_MTRR_FIX4K_C8000
			PRINT_FIXRANGE_MTRR(0x26a, 0xfff); // IA32_MTRR_FIX4K_D0000
			PRINT_FIXRANGE_MTRR(0x26b, 0xfff); // IA32_MTRR_FIX4K_D8000
			PRINT_FIXRANGE_MTRR(0x26c, 0xfff); // IA32_MTRR_FIX4K_E0000
			PRINT_FIXRANGE_MTRR(0x26d, 0xfff); // IA32_MTRR_FIX4K_E8000
			PRINT_FIXRANGE_MTRR(0x26e, 0xfff); // IA32_MTRR_FIX4K_F0000
			PRINT_FIXRANGE_MTRR(0x26f, 0xfff); // IA32_MTRR_FIX4K_F8000

#define MTRR_PHY_TEST(n) \
	ReadMSR(0x201 + (n << 1)); \
	if (msr.Lo & (1 << 11)){\
		log_info "[MTRR_DEBUG] PHY...... IA32_MTRR_PHYSMASK%d %x %x", n, msr.Hi, msr.Lo log_end \
		ReadMSR(0x200 + (n << 1)); \
		log_info "[MTRR_DEBUG] PHY...... IA32_MTRR_PHYBASE%d %x %x", n, msr.Hi, msr.Lo log_end\
	}

			Log("[MTRR_DEBUG] MTRR CUSTOM AREA!", 0x0);
			MTRR_PHY_TEST(0); // IA32_MTRR_PHYSMASK0
			MTRR_PHY_TEST(1); // IA32_MTRR_PHYSMASK1
			MTRR_PHY_TEST(2); // IA32_MTRR_PHYSMASK2
			MTRR_PHY_TEST(3); // IA32_MTRR_PHYSMASK3
			MTRR_PHY_TEST(4); // IA32_MTRR_PHYSMASK4
			MTRR_PHY_TEST(5); // IA32_MTRR_PHYSMASK5
			MTRR_PHY_TEST(6); // IA32_MTRR_PHYSMASK6
			MTRR_PHY_TEST(7); // IA32_MTRR_PHYSMASK7
			MTRR_PHY_TEST(8); // IA32_MTRR_PHYSMASK8

		}
	}

	//	(2)	Determine the VMX capabilities supported by the processor through
	//		the VMX capability MSRs.
	__asm
	{
		PUSHAD

			MOV		ECX, IA32_VMX_BASIC_MSR_CODE
			RDMSR
			LEA		EBX, vmxBasicMsr
			MOV[EBX + 4], EDX
			MOV[EBX], EAX

			MOV		ECX, IA32_FEATURE_CONTROL_CODE
			RDMSR
			LEA		EBX, vmxFeatureControl
			MOV[EBX + 4], EDX
			MOV[EBX], EAX

			POPAD
	};

	//	(3)	Create a VMXON region in non-pageable memory of a size specified by
	//		IA32_VMX_BASIC_MSR and aligned to a 4-byte boundary. The VMXON region
	//		must be hosted in cache-coherent memory.
	Log("VMXON Region Size", vmxBasicMsr.szVmxOnRegion);
	Log("VMXON Access Width Bit", vmxBasicMsr.PhyAddrWidth);
	Log("      [   1] --> 32-bit", 0);
	Log("      [   0] --> 64-bit", 0);
	Log("VMXON Memory Type", vmxBasicMsr.MemType);
	Log("      [   0]  --> Strong Uncacheable", 0);
	Log("      [ 1-5]  --> Unused", 0);
	Log("      [   6]  --> Write Back", 0);
	Log("      [7-15]  --> Unused", 0);

	_vcpu.vmxon_region_size = vmxBasicMsr.szVmxOnRegion;


	switch (vmxBasicMsr.MemType)
	{
	case 0:
		Log("Unsupported memory type.", vmxBasicMsr.MemType);
		goto Abort;
		break;
	case 6:
		break;
	default:
		Log("ERROR : Unknown VMXON Region memory type.", 0);
		goto Abort;
		break;
	}

	//	(4)	Initialize the version identifier in the VMXON region (first 32 bits)
	//		with the VMCS revision identifier reported by capability MSRs.
	*(_vcpu.vmxon_region) = vmxBasicMsr.RevId;

	Log("vmxBasicMsr.RevId", vmxBasicMsr.RevId);

	//	(5)	Ensure the current processor operating mode meets the required CR0
	//		fixed bits (CR0.PE=1, CR0.PG=1). Other required CR0 fixed bits can
	//		be detected through the IA32_VMX_CR0_FIXED0 and IA32_VMX_CR0_FIXED1
	//		MSRs.
	__asm
	{
		PUSH	EAX

			MOV		EAX, CR0
			MOV		cr0_reg, EAX

			POP		EAX
	}
	if (cr0_reg.PE != 1)
	{
		Log("ERROR : Protected Mode not enabled.", 0);
		Log("Value of CR0", cr0_reg);
		goto Abort;
	}

	Log("Protected Mode enabled.", 0);

	if (cr0_reg.PG != 1)
	{
		Log("ERROR : Paging not enabled.", 0);
		Log("Value of CR0", cr0_reg);
		goto Abort;
	}

	Log("Paging enabled.", 0);

	cr0_reg.NE = 1;

	__asm
	{
		PUSH	EAX

			MOV		EAX, cr0_reg
			MOV		CR0, EAX

			POP		EAX
	}

	//	(6)	Enable VMX operation by setting CR4.VMXE=1 [bit 13]. Ensure the
	//		resultant CR4 value supports all the CR4 fixed bits reported in
	//		the IA32_VMX_CR4_FIXED0 and IA32_VMX_CR4_FIXED1 MSRs.
	__asm
	{
		PUSH	EAX

			_emit	0x0F	// MOV	EAX, CR4
			_emit	0x20
			_emit	0xE0

			MOV		cr4_reg, EAX

			POP		EAX
	}

	Log("CR4", cr4_reg);
	cr4_reg.VMXE = 1;
	Log("CR4", cr4_reg);

	__asm
	{
		PUSH	EAX

			MOV		EAX, cr4_reg

			_emit	0x0F	// MOV	CR4, EAX
			_emit	0x22
			_emit	0xE0

			POP		EAX
	}

	//	(7)	Ensure that the IA32_FEATURE_CONTROL_MSR (MSR index 0x3A) has been
	//		properly programmed and that its lock bit is set (bit 0=1). This MSR
	//		is generally configured by the BIOS using WRMSR.
	Log("IA32_FEATURE_CONTROL Lock Bit", vmxFeatureControl.Lock);
	if (vmxFeatureControl.Lock != 1)
	{
		Log("ERROR : Feature Control Lock Bit != 1.", 0);
		goto Abort;
	}

	//	(8)	Execute VMXON with the physical address of the VMXON region as the
	//		operand. Check successful execution of VMXON by checking if
	//		RFLAGS.CF=0.
	__asm
	{
		PUSH	DWORD PTR 0
			PUSH	DWORD PTR _vcpu.vmxon_region_physical.LowPart

			_emit	0xF3	// VMXON [ESP]
			_emit	0x0F
			_emit	0xC7
			_emit	0x34
			_emit	0x24

			PUSHFD
			POP		eFlags

			ADD		ESP, 8
	}
	if (eFlags.CF == 1)
	{
		Log("ERROR : VMXON operation failed.", 0);
		goto Abort;
	}

	Log("SUCCESS : VMXON operation completed.", 0);
	Log("VMM is now running.", 0);

	//
	//	***	The processor is now in VMX root operation!
	//

	//	(1)	Create a VMCS region in non-pageable memory of size specified by
	//		the VMX capability MSR IA32_VMX_BASIC and aligned to 4-KBytes.
	//		Software should read the capability MSRs to determine width of the 
	//		physical addresses that may be used for a VMCS region and ensure
	//		the entire VMCS region can be addressed by addresses with that width.
	//		The term "guest-VMCS address" refers to the physical address of the
	//		new VMCS region for the following steps.
	_vcpu.vmcs_region_size = vmxBasicMsr.szVmxOnRegion;

	switch (vmxBasicMsr.MemType)
	{
	case 0:
		Log("Unsupported memory type.", vmxBasicMsr.MemType);
		goto Abort;
		break;
	case 6:
		break;
	default:
		Log("ERROR : Unknown VMCS Region memory type.", 0);
		goto Abort;
		break;
	}

	//	(2)	Initialize the version identifier in the VMCS (first 32 bits)
	//		with the VMCS revision identifier reported by the VMX
	//		capability MSR IA32_VMX_BASIC.
	*(_vcpu.vmcs_region) = vmxBasicMsr.RevId;

	//	(3)	Execute the VMCLEAR instruction by supplying the guest-VMCS address.
	//		This will initialize the new VMCS region in memory and set the launch
	//		state of the VMCS to "clear". This action also invalidates the
	//		working-VMCS pointer register to FFFFFFFF_FFFFFFFFH. Software should
	//		verify successful execution of VMCLEAR by checking if RFLAGS.CF = 0
	//		and RFLAGS.ZF = 0.
	__asm
	{
		PUSH	DWORD PTR 0
			PUSH	DWORD PTR _vcpu.vmcs_region_physical.LowPart
			_emit	0x66	// VMCLEAR [ESP]
			_emit	0x0F
			_emit	0xc7
			_emit	0x34
			_emit	0x24

			ADD		ESP, 8

			PUSHFD
			POP		eFlags
	}
	if (eFlags.CF != 0 || eFlags.ZF != 0)
	{
		Log("ERROR : VMCLEAR operation failed.", 0);
		goto Abort;
	}

	Log("SUCCESS : VMCLEAR operation completed.", 0);

	//	(4)	Execute the VMPTRLD instruction by supplying the guest-VMCS address.
	//		This initializes the working-VMCS pointer with the new VMCS region뭩
	//		physical address.
	__asm
	{
		PUSH	DWORD PTR 0
			PUSH	DWORD PTR _vcpu.vmcs_region_physical.LowPart

			_emit	0x0F	// VMPTRLD [ESP]
			_emit	0xC7
			_emit	0x34
			_emit	0x24

			ADD		ESP, 8
	}

	//
	//	***************************************
	//  *                                     *
	//	*	H.1.1 16-Bit Guest-State Fields   *
	//  *                                     *
	//	***************************************
	//
	//			Guest ES selector									00000800H
	__asm	MOV		seg_selector, ES
	Log("Setting Guest ES Selector", seg_selector);
	WriteVMCS(0x00000800, seg_selector);

	//			Guest CS selector									00000802H
	__asm	MOV		seg_selector, CS
	Log("Setting Guest CS Selector", seg_selector);
	WriteVMCS(0x00000802, seg_selector);

	//			Guest SS selector									00000804H
	__asm	MOV		seg_selector, SS
	Log("Setting Guest SS Selector", seg_selector);
	WriteVMCS(0x00000804, seg_selector);

	//			Guest DS selector									00000806H
	__asm	MOV		seg_selector, DS
	Log("Setting Guest DS Selector", seg_selector);
	WriteVMCS(0x00000806, seg_selector);

	//			Guest FS selector									00000808H
	__asm	MOV		seg_selector, FS
	Log("Setting Guest FS Selector", seg_selector);
	WriteVMCS(0x00000808, seg_selector);

	//			Guest GS selector									0000080AH
	__asm	MOV		seg_selector, GS
	Log("Setting Guest GS Selector", seg_selector);
	WriteVMCS(0x0000080A, seg_selector);

	//			Guest TR selector									0000080EH
	__asm	STR		seg_selector
	ClearBit((ULONG*)&seg_selector, 2);						// TI Flag
	Log("Setting Guest TR Selector", seg_selector);
	WriteVMCS(0x0000080E, seg_selector);

	//	**************************************
	//  *                                    *
	//	*	H.1.2 16-Bit Host-State Fields   *
	//  *                                    *
	//	**************************************
	//
	//			Host ES selector									00000C00H
	__asm	MOV		seg_selector, ES
	seg_selector &= 0xFFFC;
	Log("Setting Host ES Selector", seg_selector);
	WriteVMCS(0x00000C00, seg_selector);

	//			Host CS selector									00000C02H
	__asm	MOV		seg_selector, CS
	Log("Setting Host CS Selector", seg_selector);
	WriteVMCS(0x00000C02, seg_selector);

	//			Host SS selector									00000C04H
	__asm	MOV		seg_selector, SS
	Log("Setting Host SS Selector", seg_selector);
	WriteVMCS(0x00000C04, seg_selector);

	//			Host DS selector									00000C06H
	__asm	MOV		seg_selector, DS
	seg_selector &= 0xFFFC;
	Log("Setting Host DS Selector", seg_selector);
	WriteVMCS(0x00000C06, seg_selector);

	//			Host FS selector									00000C08H
	__asm	MOV		seg_selector, FS
	Log("Setting Host FS Selector", seg_selector);
	WriteVMCS(0x00000C08, seg_selector);

	//			Host GS selector									00000C0AH
	__asm	MOV		seg_selector, GS
	seg_selector &= 0xFFFC;
	Log("Setting Host GS Selector", seg_selector);
	WriteVMCS(0x00000C0A, seg_selector);

	//			Host TR selector									00000C0CH
	__asm	STR		seg_selector
	Log("Setting Host TR Selector", seg_selector);
	WriteVMCS(0x00000C0C, seg_selector);

	//	***************************************
	//  *                                     *
	//	*	H.2.2 64-Bit Guest-State Fields   *
	//  *                                     *
	//	***************************************
	//
	//			VMCS Link Pointer (full)							00002800H
	temp32 = 0xFFFFFFFF;
	Log("Setting VMCS Link Pointer (full)", temp32);
	WriteVMCS(0x00002800, temp32);

	//			VMCS link pointer (high)							00002801H
	temp32 = 0xFFFFFFFF;
	Log("Setting VMCS Link Pointer (high)", temp32);
	WriteVMCS(0x00002801, temp32);

	//			Reserved Bits of IA32_DEBUGCTL MSR must be 0
	//			(1D9H)
	ReadMSR(0x000001D9);
	Log("IA32_DEBUGCTL MSR", msr.Lo);

	//			Guest IA32_DEBUGCTL (full)							00002802H
	temp32 = msr.Lo;
	Log("Setting Guest IA32_DEBUGCTL (full)", temp32);
	WriteVMCS(0x00002802, temp32);

	//			Guest IA32_DEBUGCTL (high)							00002803H
	temp32 = msr.Hi;
	Log("Setting Guest IA32_DEBUGCTL (high)", temp32);
	WriteVMCS(0x00002803, temp32);

	//	***********************************
	//  *                                 *
	//	*	H.3.1 32-Bit Control Fields   *
	//  *                                 *
	//	***********************************
	//
	//			Pin-based VM-execution controls						00004000H
	//			IA32_VMX_PINBASED_CTLS MSR (index 481H)
	ReadMSR(0x481);
	Log("Pin-based allowed-0", msr.Lo);
	Log("Pin-based allowed-1", msr.Hi);
	temp32 = 0;
	temp32 |= msr.Lo;
	temp32 &= msr.Hi;
	//SetBit( &temp32, 3 );
	//set_bit32(&temp32, 0); // enable interrypt virtual
	Log("Setting Pin-Based Controls Mask", temp32);
	WriteVMCS(0x00004000, temp32);

	//			Primary processor-based VM-execution controls		00004002H
	//			IA32_VMX_PROCBASED_CTLS MSR (index 482H)
	ReadMSR(0x482);
	Log("Proc-based allowed-0", msr.Lo);
	Log("Proc-based allowed-1", msr.Hi);
	temp32 = 0;
	temp32 |= msr.Lo;
	temp32 &= msr.Hi;

	//> somma
	//set_bit32(&temp32, PROC_BASED_USE_IOBITMAP);				// Use I/O bitmaps
	//set_bit32(&temp32, PROC_BASED_HLT_EXITING);					// HLT

	// enable Secondary VM Execution control
	set_bit32(&temp32, 31);
	// enable interrupt virtual


	Log("Setting Pri Proc-Based Controls Mask", temp32);
	WriteVMCS(0x00004002, temp32);

	// IA32_VMX_PROCBASED_CTLS2 (index 48Bh )
	// bit 1 is EPT 

	// Secondary VM Execution control encoding 401eH
	ReadMSR(0x48b);
	temp32 = 0;
	//temp32 |= msr.Lo;
	//temp32 &= msr.Hi;

	set_bit32(&temp32, 1); // enable EPT
	//set_bit32(&temp32, 18); // enable EPT #VE
	//set_bit32(&temp32, 5); // enable VPID


	Log("Setting Sec Proc-Based Controls Mask", temp32);
	WriteVMCS(0x401e, temp32);

	//temp32 = 1;
	//WriteVMCS(0, temp32);// VPID = 1



	///*
	//*((char*)_vcpu.io_bitmap_a + (0x60/8)) = 0x11;
	//*/
	//*(_vcpu.io_bitmap_a + (0x60/8)) = 0x11;
	*((DWORD*)(_vcpu.io_bitmap_a)) = (1 << 14);
	WriteVMCS(IO_BITMAP_A_HIGH, _vcpu.io_bitmap_a_physical.HighPart);
	WriteVMCS(IO_BITMAP_A, _vcpu.io_bitmap_a_physical.LowPart);
	WriteVMCS(IO_BITMAP_B_HIGH, _vcpu.io_bitmap_b_physical.HighPart);
	WriteVMCS(IO_BITMAP_B, _vcpu.io_bitmap_b_physical.LowPart);




	//	Get the CR3-target count, MSR store/load counts, et cetera
	//
	//	IA32_VMX_MISC MSR (index 485H)
	ReadMSR(0x485);
	Log("Misc Data", msr.Lo);
	//Log( "Misc Data" , msr.Hi );
	RtlCopyBytes(&misc_data, &msr.Lo, 4);
	Log("   ActivityStates", misc_data.ActivityStates);
	Log("   CR3Targets", misc_data.CR3Targets);
	Log("   MaxMSRs", misc_data.MaxMSRs);

	//			VM-exit controls									0000400CH
	//			IA32_VMX_EXIT_CTLS MSR (index 483H)
	ReadMSR(0x483);
	Log("Exit controls allowed-0", msr.Lo);
	Log("Exit controls allowed-1", msr.Hi);
	temp32 = 0;
	temp32 |= msr.Lo;
	temp32 &= msr.Hi;
	SetBit(&temp32, 15);								// Acknowledge Interrupt On Exit
	Log("Setting VM-Exit Controls Mask", temp32);
	WriteVMCS(0x0000400C, temp32);

	//			VM-entry controls									00004012H
	//			IA32_VMX_ENTRY_CTLS MSR (index 484H)
	ReadMSR(0x484);
	Log("VMX Entry allowed-0", msr.Lo);
	Log("VMX Entry allowed-1", msr.Hi);
	temp32 = 0;
	temp32 |= msr.Lo;
	temp32 &= msr.Hi;
	ClearBit(&temp32, 9);							// IA-32e Mode Guest Disable
	Log("Setting VM-Entry Controls Mask", temp32);
	WriteVMCS(0x00004012, temp32);

	//	***************************************
	//  *                                     *
	//	*	H.3.3 32-Bit Guest-State Fields   *
	//  *                                     *
	//	***************************************
	//
	//			Guest ES limit										00004800H
	__asm	MOV seg_selector, ES
	temp32 = 0;
	temp32 = GetSegmentDescriptorLimit(gdt_base, seg_selector);
	Log("Setting Guest ES limit", 0xFFFFFFFF);
	WriteVMCS(0x00004800, 0xFFFFFFFF);

	//			Guest CS limit										00004802H
	__asm	MOV seg_selector, CS
	temp32 = 0;
	temp32 = GetSegmentDescriptorLimit(gdt_base, seg_selector);
	Log("Setting Guest CS limit", 0xFFFFFFFF);
	WriteVMCS(0x00004802, 0xFFFFFFFF);

	//			Guest SS limit										00004804H
	__asm	MOV seg_selector, SS
	temp32 = 0;
	temp32 = GetSegmentDescriptorLimit(gdt_base, seg_selector);
	Log("Setting Guest SS limit", 0xFFFFFFFF);
	WriteVMCS(0x00004804, 0xFFFFFFFF);

	//			Guest DS limit										00004806H
	__asm	MOV seg_selector, DS
	temp32 = 0;
	temp32 = GetSegmentDescriptorLimit(gdt_base, seg_selector);
	Log("Setting Guest DS limit", 0xFFFFFFFF);
	WriteVMCS(0x00004806, 0xFFFFFFFF);

	//			Guest FS limit										00004808H
	__asm	MOV seg_selector, FS
	temp32 = 0;
	temp32 = GetSegmentDescriptorLimit(gdt_base, seg_selector);
	Log("Setting Guest FS limit", 0x00001000);
	WriteVMCS(0x00004808, 0x00001000);

	//			Guest GS limit										0000480AH
	__asm	MOV seg_selector, GS
	temp32 = 0;
	temp32 = GetSegmentDescriptorLimit(gdt_base, seg_selector);
	Log("Setting Guest GS limit", 0xFFFFFFFF);
	WriteVMCS(0x0000480A, 0xFFFFFFFF);

	//			Guest TR limit										0000480EH
	__asm
	{
		PUSH	EAX

			STR		AX
			MOV		mLDT, AX

			POP		EAX
	}
	temp32 = 0;
	temp32 = GetSegmentDescriptorLimit(gdt_base, mLDT);
	Log("Setting Guest TR limit", temp32);
	WriteVMCS(0x0000480E, temp32);

	//			Guest GDTR limit									00004810H
	Log("Setting Guest GDTR limit", gdt_reg.Limit);
	WriteVMCS(0x00004810, gdt_reg.Limit);

	//			Guest IDTR limit									00004812H
	Log("Setting Guest IDTR limit", idt_reg.Limit);
	WriteVMCS(0x00004812, idt_reg.Limit);

	__asm	MOV		seg_selector, CS
	temp32 = seg_selector;
	temp32 >>= 3;
	temp32 *= 8;
	temp32 += (gdt_base + 5);			// CS Segment Descriptor
	__asm
	{
		PUSHAD
			MOV		EAX, temp32
			MOV		EBX, [EAX]
			MOV		temp32, EBX
			POPAD
	}
	temp32 &= 0x0000F0FF;
	Log("Setting Guest CS access rights", temp32);
	WriteVMCS(0x00004816, temp32);

	__asm	MOV		seg_selector, DS
	temp32 = seg_selector;
	temp32 >>= 3;
	temp32 *= 8;
	temp32 += (gdt_base + 5);			// DS Segment Descriptor
	__asm
	{
		PUSHAD
			MOV		EAX, temp32
			MOV		EBX, [EAX]
			MOV		temp32, EBX
			POPAD
	}
	temp32 &= 0x0000F0FF;
	Log("Setting Guest DS access rights", temp32);
	WriteVMCS(0x0000481A, temp32);

	__asm	MOV		seg_selector, ES
	temp32 = seg_selector;
	temp32 >>= 3;
	temp32 *= 8;
	temp32 += (gdt_base + 5);			// ES Segment Descriptor
	__asm
	{
		PUSHAD
			MOV		EAX, temp32
			MOV		EBX, [EAX]
			MOV		temp32, EBX
			POPAD
	}
	temp32 &= 0x0000F0FF;
	Log("Setting Guest ES access rights", temp32);
	WriteVMCS(0x00004814, temp32);

	__asm	MOV		seg_selector, FS
	temp32 = seg_selector;
	temp32 >>= 3;
	temp32 *= 8;
	temp32 += (gdt_base + 5);			// FS Segment Descriptor
	__asm
	{
		PUSHAD
			MOV		EAX, temp32
			MOV		EBX, [EAX]
			MOV		temp32, EBX
			POPAD
	}
	temp32 &= 0x0000F0FF;
	temp32 &= 0xFFFF7FFF;				// Granularity Bit = 0
	Log("Setting Guest FS access rights", temp32);
	WriteVMCS(0x0000481C, temp32);

	__asm	MOV		seg_selector, GS
	temp32 = seg_selector;
	temp32 >>= 3;
	temp32 *= 8;
	temp32 += (gdt_base + 5);			// GS Segment Descriptor
	__asm
	{
		PUSHAD
			MOV		EAX, temp32
			MOV		EBX, [EAX]
			MOV		temp32, EBX
			POPAD
	}
	temp32 &= 0x0000F0FF;
	SetBit(&temp32, 16);				// Unusable
	Log("Setting Guest GS access rights", temp32);
	WriteVMCS(0x0000481E, temp32);

	__asm	MOV		seg_selector, SS
	temp32 = seg_selector;
	temp32 >>= 3;
	temp32 *= 8;
	temp32 += (gdt_base + 5);			// SS Segment Descriptor
	__asm
	{
		PUSHAD
			MOV		EAX, temp32
			MOV		EBX, [EAX]
			MOV		temp32, EBX
			POPAD
	}
	temp32 &= 0x0000F0FF;
	Log("Setting Guest SS access rights", temp32);
	WriteVMCS(0x00004818, temp32);

	__asm	STR		seg_selector
	temp32 = seg_selector;
	temp32 >>= 3;
	temp32 *= 8;
	temp32 += (gdt_base + 5);			// TR Segment Descriptor
	__asm
	{
		PUSHAD
			MOV		EAX, temp32
			MOV		EBX, [EAX]
			MOV		temp32, EBX
			POPAD
	}
	temp32 &= 0x0000F0FF;
	Log("Setting Guest TR access rights", temp32);
	WriteVMCS(0x00004822, temp32);

	//			Guest LDTR access rights							00004820H
	temp32 = 0;
	SetBit(&temp32, 16);			// Unusable
	Log("Setting Guest LDTR access rights", temp32);
	WriteVMCS(0x00004820, temp32);

	//			Guest IA32_SYSENTER_CS								0000482AH
	//			(174H)
	ReadMSR(0x174);
	Log("Setting Guest IA32_SYSENTER_CS", (ULONG)msr.Lo);
	WriteVMCS(0x0000482A, msr.Lo);

	//	**************************************
	//  *                                    *
	//	*	H.3.4 32-Bit Host-State Fields   *
	//  *                                    *
	//	**************************************
	//
	//			Host IA32_SYSENTER_CS								00004C00H
	//			(174H)
	ReadMSR(0x174);
	Log("Setting Host IA32_SYSENTER_CS", (ULONG)msr.Lo);
	WriteVMCS(0x00004C00, msr.Lo);

	//	**********************************************
	//  *                                            *
	//	*	H.4.3 Natural-Width Guest-State Fields   *
	//  *                                            *
	//	**********************************************
	//
	//			Guest CR0											00006800H
	__asm
	{
		PUSH	EAX
			MOV		EAX, CR0
			MOV		temp32, EAX
			POP		EAX
	}


	ReadMSR(0x486);							// IA32_VMX_CR0_FIXED0
	Log("IA32_VMX_CR0_FIXED0", msr.Lo);

	ReadMSR(0x487);							// IA32_VMX_CR0_FIXED1
	Log("IA32_VMX_CR0_FIXED1", msr.Lo);

	SetBit(&temp32, 0);		// PE
	SetBit(&temp32, 5);		// NE
	SetBit(&temp32, 31);		// PG
	Log("Setting Guest CR0", temp32);
	WriteVMCS(0x00006800, temp32);

	//			Guest CR3											00006802H
	__asm
	{
		PUSH	EAX

			_emit	0x0F	// MOV EAX, CR3
			_emit	0x20
			_emit	0xD8

			MOV		temp32, EAX

			POP		EAX
	}
	Log("Setting Guest CR3", temp32);
	WriteVMCS(0x00006802, temp32);

	//			Guest CR4											00006804H
	__asm
	{
		PUSH	EAX

			_emit	0x0F	// MOV EAX, CR4
			_emit	0x20
			_emit	0xE0

			MOV		temp32, EAX

			POP		EAX
	}

	ReadMSR(0x488);							// IA32_VMX_CR4_FIXED0
	Log("IA32_VMX_CR4_FIXED0", msr.Lo);

	ReadMSR(0x489);							// IA32_VMX_CR4_FIXED1
	Log("IA32_VMX_CR4_FIXED1", msr.Lo);

	SetBit(&temp32, 13);		// VMXE
	Log("Setting Guest CR4", temp32);
	WriteVMCS(0x00006804, temp32);

	//			Guest ES base										00006806H
	__asm	MOV		seg_selector, ES
	temp32 = 0;
	temp32 = GetSegmentDescriptorBase(gdt_base, seg_selector);
	Log("Setting Guest ES Base", temp32);
	WriteVMCS(0x00006806, temp32);

	//			Guest CS base										00006808H
	__asm	MOV		seg_selector, CS
	temp32 = 0;
	temp32 = GetSegmentDescriptorBase(gdt_base, seg_selector);
	Log("Setting Guest CS Base", temp32);
	WriteVMCS(0x00006808, temp32);

	//			Guest SS base										0000680AH
	__asm	MOV		seg_selector, SS
	temp32 = 0;
	temp32 = GetSegmentDescriptorBase(gdt_base, seg_selector);
	Log("Setting Guest SS Base", temp32);
	WriteVMCS(0x0000680A, temp32);

	//			Guest DS base										0000680CH
	__asm	MOV		seg_selector, DS
	temp32 = 0;
	temp32 = GetSegmentDescriptorBase(gdt_base, seg_selector);
	Log("Setting Guest DS Base", temp32);
	WriteVMCS(0x0000680C, temp32);

	//			Guest FS base										0000680EH
	__asm	MOV		seg_selector, FS
	temp32 = 0;
	temp32 = GetSegmentDescriptorBase(gdt_base, seg_selector);
	Log("Setting Guest FS Base", temp32);
	WriteVMCS(0x0000680E, temp32);

	//			Guest TR base										00006814H
	__asm
	{
		PUSH	EAX

			STR		AX
			MOV		mLDT, AX

			POP		EAX
	}
	temp32 = 0;
	temp32 = GetSegmentDescriptorBase(gdt_base, mLDT);
	Log("Setting Guest TR Base", temp32);
	WriteVMCS(0x00006814, temp32);

	//			Guest GDTR base										00006816H
	__asm
	{
		SGDT	gdt_reg
	}
	temp32 = 0;
	temp32 = gdt_reg.BaseHi;
	temp32 <<= 16;
	temp32 |= gdt_reg.BaseLo;
	Log("Setting Guest GDTR Base", temp32);
	WriteVMCS(0x00006816, temp32);

	//			Guest IDTR base										00006818H
	__asm
	{
		SIDT	idt_reg
	}
	temp32 = 0;
	temp32 = idt_reg.BaseHi;
	temp32 <<= 16;
	temp32 |= idt_reg.BaseLo;
	Log("Setting Guest IDTR Base", temp32);
	WriteVMCS(0x00006818, temp32);

	//			Guest RFLAGS										00006820H
	__asm
	{
		PUSHAD

			PUSHFD

			MOV		EAX, 0x00006820

			// VMWRITE	EAX, [ESP]
			_emit	0x0F
			_emit	0x79
			_emit	0x04
			_emit	0x24

			POP		eFlags

			POPAD
	}
	Log("Guest EFLAGS", eFlags);

	//			Guest IA32_SYSENTER_ESP								00006824H
	//			MSR (175H)
	ReadMSR(0x175);
	Log("Setting Guest IA32_SYSENTER_ESP", msr.Lo);
	WriteVMCS(0x00006824, msr.Lo);

	//			Guest IA32_SYSENTER_EIP								00006826H
	//			MSR (176H)
	ReadMSR(0x176);
	Log("Setting Guest IA32_SYSENTER_EIP", msr.Lo);
	WriteVMCS(0x00006826, msr.Lo);


	//	*********************************************
	//  *                                           *
	//	*	H.4.4 Natural-Width Host-State Fields   *
	//  *                                           *
	//	*********************************************
	//
	//			Host CR0											00006C00H
	__asm
	{
		PUSH	EAX
			MOV		EAX, CR0
			MOV		temp32, EAX
			POP		EAX
	}
	SetBit(&temp32, 5);								// Set NE Bit
	Log("Setting Host CR0", temp32);
	WriteVMCS(0x00006C00, temp32);

	//			Host CR3											00006C02H
	__asm
	{
		PUSH	EAX

			_emit	0x0F	// MOV EAX, CR3
			_emit	0x20
			_emit	0xD8

			MOV		temp32, EAX

			POP		EAX
	}
	Log("Setting Host CR3", temp32);
	WriteVMCS(0x00006C02, temp32);

	//			Host CR4											00006C04H
	__asm
	{
		PUSH	EAX

			_emit	0x0F	// MOV EAX, CR4
			_emit	0x20
			_emit	0xE0

			MOV		temp32, EAX

			POP		EAX
	}
	Log("Setting Host CR4", temp32);
	WriteVMCS(0x00006C04, temp32);

	//			Host FS base										00006C06H
	__asm	MOV		seg_selector, FS
	temp32 = 0;
	temp32 = GetSegmentDescriptorBase(gdt_base, seg_selector);
	Log("Setting Host FS Base", temp32);
	WriteVMCS(0x00006C06, temp32);

	//			Host TR base										00006C0AH
	__asm
	{
		PUSH	EAX

			STR		AX
			MOV		mLDT, AX

			POP		EAX
	}
	temp32 = 0;
	temp32 = GetSegmentDescriptorBase(gdt_base, mLDT);
	Log("Setting Host TR Base", temp32);
	WriteVMCS(0x00006C0A, temp32);

	//			Host GDTR base										00006C0CH
	__asm
	{
		SGDT	gdt_reg
	}
	temp32 = 0;
	temp32 = gdt_reg.BaseHi;
	temp32 <<= 16;
	temp32 |= gdt_reg.BaseLo;
	Log("Setting Host GDTR Base", temp32);
	WriteVMCS(0x00006C0C, temp32);

	//			Host IDTR base										00006C0EH
	__asm
	{
		SIDT	idt_reg
	}
	temp32 = 0;
	temp32 = idt_reg.BaseHi;
	temp32 <<= 16;
	temp32 |= idt_reg.BaseLo;
	Log("Setting Host IDTR Base", temp32);
	WriteVMCS(0x00006C0E, temp32);

	//			Host IA32_SYSENTER_ESP								00006C10H
	//			MSR (175H)
	ReadMSR(0x175);
	Log("Setting Host IA32_SYSENTER_ESP", msr.Lo);
	WriteVMCS(0x00006C10, msr.Lo);

	//			Host IA32_SYSENTER_EIP								00006C12H
	//			MSR (176H)
	ReadMSR(0x176);
	Log("Setting Host IA32_SYSENTER_EIP", msr.Lo);
	WriteVMCS(0x00006C12, msr.Lo);

	//	(5)	Issue a sequence of VMWRITEs to initialize various host-state area
	//		fields in the working VMCS. The initialization sets up the context
	//		and entry-points to the VMM VIRTUAL-MACHINE MONITOR PROGRAMMING
	//		CONSIDERATIONS upon subsequent VM exits from the guest. Host-state
	//		fields include control registers (CR0, CR3 and CR4), selector fields
	//		for the segment registers (CS, SS, DS, ES, FS, GS and TR), and base-
	//		address fields (for FS, GS, TR, GDTR and IDTR; RSP, RIP and the MSRs
	//		that control fast system calls).
	//		

	//	(6)	Use VMWRITEs to set up the various VM-exit control fields, VM-entry
	//		control fields, and VM-execution control fields in the VMCS. Care
	//		should be taken to make sure the settings of individual fields match
	//		the allowed 0 and 1 settings for the respective controls as reported
	//		by the VMX capability MSRs (see Appendix G). Any settings inconsistent
	//		with the settings reported by the capability MSRs will cause VM
	//		entries to fail.

	//	(7)	Use VMWRITE to initialize various guest-state area fields in the
	//		working VMCS. This sets up the context and entry-point for guest
	//		execution upon VM entry. Chapter 22 describes the guest-state loading
	//		and checking done by the processor for VM entries to protected and
	//		virtual-8086 guest execution.
	//

	// Clear the VMX Abort Error Code prior to VMLAUNCH
	//
	RtlZeroMemory((_vcpu.vmcs_region + 4), 4);
	Log("Clearing VMX Abort Error Code", *(_vcpu.vmcs_region + 4));

	//	Set EIP, ESP for the Guest right before calling VMLAUNCH
	//
	Log("Setting Guest ESP", GuestStack);
	WriteVMCS(0x0000681C, (ULONG)GuestStack);

	Log("Setting Guest EIP", GuestReturn);
	WriteVMCS(0x0000681E, (ULONG)GuestReturn);

	//	Set EIP, ESP for the Host right before calling VMLAUNCH
	//
	Log("Setting Host ESP", ((ULONG)_vcpu.fake_stack + 0x1FFF));
	WriteVMCS(0x00006C14, ((ULONG)_vcpu.fake_stack + 0x1FFF));

	Log("Setting Host EIP", VMMEntryPoint);
	WriteVMCS(0x00006C16, (ULONG)VMMEntryPoint);

	/*

	Enable EPT
	64bit control
	*/

	__asm{
		mov eax, 0x80000008
			cpuid
			mov temp32, eax
	}
	Log("Physical Address Width", temp32 & 0x7f);

	ReadMSR(0x48c);
	Log("IA32_VMX_EPT_VPID_CAP Lo", msr.Lo);
	Log("IA32_VMX_EPT_VPID_CAP Hi", msr.Hi);

	if (msr.Lo & (1 << 0)) Log("Execute Only Support", msr.Lo);
	if (msr.Lo & (1 << 6)) Log("Page walk support 4", msr.Lo);
	if (msr.Lo & (1 << 8)) Log("Support Mem type UC", msr.Lo);
	if (msr.Lo & (1 << 14)) Log("Support Mem type WB", msr.Lo);
	if (msr.Lo & (1 << 16)) Log("Support Mem mapping 2M", msr.Lo);
	if (msr.Lo & (1 << 17)) Log("Support Mem mapping 1G", msr.Lo);


	//Log("Setting EPTP Full")
	//temp64 = _vcpu.ept_physical.QuadPart | (0x3 << 0x3) | (6);
	temp32 = (ULONG)_vcpu.ept_physical.LowPart;
	temp32 = temp32 | (0x3 << 0x3)/*walk length 4*/ | (6) /*mem type WB*/;

	Log("EPTP Low Addresssssssssssssssssssssssssssss", temp32);
	WriteVMCS(0x201a, temp32);

	temp32 = (ULONG)_vcpu.ept_physical.HighPart;
	WriteVMCS(0x201b, temp32);



	//WriteVMCS(0x280b, _vcpu.ept_pdpte_physical.HighPart);
	//WriteVMCS(0x280a, _vcpu.ept_pdpte_physical.LowPart);


	//temp32 = (ULONG)_vcpu.ept_physical.HighPart;
	//Log("Setting EPTP High", temp32 );
	//WriteVMCS(0x201b, temp32);


	/*

	Enable EPT END

	*/

	////////////////
	//            //
	//	VMLAUNCH  //
	//            //
	////////////////
	__asm
	{
		_emit	0x0F	// VMLAUNCH
			_emit	0x01
			_emit	0xC2
	}

	__asm
	{
		PUSHFD
			POP		eFlags
	}

	Log("VMLAUNCH Failure", 0xDEADF00D)

	if (eFlags.CF != 0 || eFlags.ZF != 0 || TRUE)
	{
		//
		//	Get the ERROR number using VMCS field 00004400H
		//
		__asm
		{
			PUSHAD

				MOV		EAX, 0x00004400

				_emit	0x0F	// VMREAD  EBX, EAX
				_emit	0x78
				_emit	0xC3

				MOV		ErrorCode, EBX

				POPAD
		}

		Log("VM Instruction Error", ErrorCode);
	}

Abort:

	ScrubTheLaunch = 1;
	__asm
	{
		MOV		ESP, GuestStack
			JMP		GuestReturn
	}
}























ULONG		ExitReason;
ULONG		ExitQualification;
ULONG		ExitInterruptionInformation;
ULONG		ExitInterruptionErrorCode;
ULONG		IDTVectoringInformationField;
ULONG		IDTVectoringErrorCode;
ULONG		ExitInstructionLength;
ULONG		ExitInstructionInformation;

ULONG		GuestEIP;
ULONG		GuestResumeEIP;
ULONG		GuestESP;
ULONG		GuestCS;
ULONG		GuestCR0;
ULONG		GuestCR3;
ULONG		GuestCR4;
ULONG		GuestEFLAGS;

ULONG		GuestEAX;
ULONG		GuestEBX;
ULONG		GuestECX;
ULONG		GuestEDX;
ULONG		GuestEDI;
ULONG		GuestESI;
ULONG		GuestEBP;

ULONG		movcrControlRegister;
ULONG		movcrAccessType;
ULONG		movcrOperandType;
ULONG		movcrGeneralPurposeRegister;
ULONG		movcrLMSWSourceData;

//ULONG		ErrorCode;

///////////////////////
//                   //
//  VMM Entry Point  //
//                   //
///////////////////////

#define LOG_REGISTER( reginame ) Log("Guest" #reginame, Guest##reginame)
void print_guest_register()
{
	Log("=================================================", 0x00000000);
	LOG_REGISTER(EAX);
	LOG_REGISTER(EBX);
	LOG_REGISTER(ECX);
	LOG_REGISTER(EDX);
	LOG_REGISTER(EDI);
	LOG_REGISTER(ESI);
	LOG_REGISTER(EBP);

	__asm
	{
		PUSHAD

			MOV		EAX, 0x0000681C

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestESP, EBX

			POPAD
	}
	LOG_REGISTER(ESP);

	__asm
	{
		PUSHAD

			MOV		EAX, 0x0000681E

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestEIP, EBX

			POPAD
	}

	LOG_REGISTER(EIP);
	Log("=================================================", 0xffffffff);
}
__declspec(naked) VOID VMMEntryPoint()
{
	__asm	CLI

	__asm	PUSHAD

	//
	//	Record the General-Purpose registers.
	//
	__asm	MOV GuestEAX, EAX
	__asm	MOV GuestEBX, EBX
	__asm	MOV GuestECX, ECX
	__asm	MOV GuestEDX, EDX
	__asm	MOV GuestEDI, EDI
	__asm	MOV GuestESI, ESI
	__asm	MOV GuestEBP, EBP

	///////////////////
	//               //
	//  Exit Reason  //		0x00004400
	//               //
	///////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00004402

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		ExitReason, EBX

			POPAD
	}

	if (ExitReason == 0x0000000A ||	// CPUID
		ExitReason == 0x00000012 || // VMCALL
		ExitReason == 0x0000001C || // MOV CR
		ExitReason == 0x0000001F || // RDMSR
		ExitReason == 0x00000020 ||	// WRMSR
		(ExitReason > 0x00000012 && ExitReason < 0x0000001C))
	{
		HandlerLogging = 0;
	}
	else
	{
		HandlerLogging = 1;
	}

	//> somma 
	HandlerLogging = 0;

	//if( HandlerLogging )
	//{
	//	Log( "----- VMM Handler CPU0 -----", 0 );
	//	Log( "Guest EAX" , GuestEAX );
	//	Log( "Guest EBX" , GuestEBX );
	//	Log( "Guest ECX" , GuestECX );
	//	Log( "Guest EDX" , GuestEDX );
	//	Log( "Guest EDI" , GuestEDI );
	//	Log( "Guest ESI" , GuestESI );
	//	Log( "Guest EBP" , GuestEBP );
	//	Log( "Exit Reason" , ExitReason );
	//}

	//////////////////////////
	//                      //
	//  Exit Qualification  //	00006400H
	//                      //
	//////////////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00006400

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		ExitQualification, EBX

			POPAD
	}
	if (HandlerLogging) Log("Exit Qualification", ExitQualification);

	////////////////////////////////////////
	//                                    //
	//  VM-Exit Interruption Information  //	00004404H
	//                                    //
	////////////////////////////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00004404

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		ExitInterruptionInformation, EBX

			POPAD
	}
	if (HandlerLogging) Log("Exit Interruption Information", ExitInterruptionInformation);

	///////////////////////////////////////
	//                                   //
	//  VM-Exit Interruption Error Code  //	00004406H
	//                                   //
	///////////////////////////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00004406

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		ExitInterruptionErrorCode, EBX

			POPAD
	}
	if (HandlerLogging) Log("Exit Interruption Error Code", ExitInterruptionErrorCode);

	///////////////////////////////////////
	//                                   //
	//  IDT-Vectoring Information Field  //	00004408H
	//                                   //
	///////////////////////////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00004408

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		IDTVectoringInformationField, EBX

			POPAD
	}
	if (HandlerLogging) Log("IDT-Vectoring Information Field", IDTVectoringInformationField);

	////////////////////////////////
	//                            //
	//  IDT-Vectoring Error Code  //	0000440AH
	//                            //
	////////////////////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x0000440A

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		IDTVectoringErrorCode, EBX

			POPAD
	}
	if (HandlerLogging) Log("IDT-Vectoring Error Code", IDTVectoringErrorCode);

	//////////////////////////////////
	//                              //
	//  VM-Exit Instruction Length  //	0000440CH
	//                              //
	//////////////////////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x0000440C

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		ExitInstructionLength, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM-Exit Instruction Length", ExitInstructionLength);

	///////////////////////////////////////
	//                                   //
	//  VM-Exit Instruction Information  //	0000440EH
	//                                   //
	///////////////////////////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x0000440E

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		ExitInstructionInformation, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM-Exit Instruction Information", ExitInstructionInformation);

	/////////////////
	//             //
	//  Guest EIP  //
	//             //
	/////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x0000681E

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestEIP, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM Exit EIP", GuestEIP);

	//
	//	Writing the Guest VMCS EIP uses general registers.
	//	Must complete this before setting general registers
	//	for guest return state.
	//
	GuestResumeEIP = GuestEIP + ExitInstructionLength;
	WriteVMCS(0x0000681E, (ULONG)GuestResumeEIP);

	/////////////////
	//             //
	//  Guest ESP  //
	//             //
	/////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x0000681C

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestESP, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM Exit ESP", GuestESP);

	////////////////
	//            //
	//  Guest CS  //
	//            //
	////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00000802

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestCS, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM Exit CS", GuestCS);

	/////////////////
	//             //
	//  Guest CR0  //
	//             //
	/////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00006800

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestCR0, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM Exit CR0", GuestCR0);

	/////////////////
	//             //
	//  Guest CR3  //
	//             //
	/////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00006802

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestCR3, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM Exit CR3", GuestCR3);

	/////////////////
	//             //
	//  Guest CR4  //
	//             //
	/////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00006804

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestCR4, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM Exit CR4", GuestCR4);

	////////////////////
	//                //
	//  Guest EFLAGS  //
	//                //
	////////////////////
	__asm
	{
		PUSHAD

			MOV		EAX, 0x00006820

			_emit	0x0F	// VMREAD  EBX, EAX
			_emit	0x78
			_emit	0xC3

			MOV		GuestEFLAGS, EBX

			POPAD
	}
	if (HandlerLogging) Log("VM Exit EFLAGS", GuestEFLAGS);

	if (ExitReason == 0x1)
	{
		Log("External Interrupt!!", (ExitInterruptionInformation & 0xff));
		if ((ExitInterruptionInformation & 0xff) == 14){ // page fault
			__asm int 3
		}

	}

	/////////////////////////////////////////////
	//                                         //
	//  *** EXIT REASON CHECKS START HERE ***  //
	//                                         //
	/////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////////////
	//                                                                                 //
	//  VMCLEAR, VMLAUNCH, VMPTRLD, VMPTRST, VMREAD, VMWRITE, VMRESUME, VMXOFF, VMXON  //
	//                                                                                 //
	/////////////////////////////////////////////////////////////////////////////////////
	if (ExitReason > 0x00000012 && ExitReason < 0x0000001C)
	{
		Log("Request has been denied - CPU0", ExitReason);

		__asm
		{
			POPAD
				JMP		Resume
		}
	}
	if (ExitReason == 0x00000002)
	{
		Log("VMX_EXIT_TRIPLE_FAULT!", 0xdeadbeef);

		__asm{
			mov eax, 0x280a
				_emit	0x0F	// VMREAD  EBX, EAX
				_emit	0x78
				_emit	0xC3

				MOV		GuestEFLAGS, EBX
		}
		Log("PDPTE0", GuestEFLAGS);

		__asm{
			mov eax, 0x201a
				_emit 0x0f
				_emit 0x78
				_emit 0xc3
				mov GuestEFLAGS, ebx
		}
		Log("0x201a", GuestEFLAGS);
		__asm{
			mov eax, 0x201b
				_emit 0x0f
				_emit 0x78
				_emit 0xc3
				mov GuestEFLAGS, ebx
		}
		Log("0x201b", GuestEFLAGS);

		print_guest_register();

		__asm{
			int 3
				popad
				jmp Resume
		}
	}
	//////////////
	//          //
	//  VMCALL  //
	//  VMX_EXIT_EPT_VIOLATION 48
	//  VMX_EXIT_EPT_MISCONFIG 49        //
	//////////////

	if (ExitReason == 48){
		Log("VMX_EXIT_VIOLATION", 0xdeadbeef);
		print_guest_register();
		__asm{
			int 3
			popad
				jmp Resume
		}
	}

	if (ExitReason == 49){
		Log("VMX_EXIT_MISCONFIG", 0xdeadbeef);
		print_guest_register();
		__asm{
			int 3
			popad
				jmp Resume
		}
	}


	//////////////
	//          //
	//  VMCALL  //
	//          //
	//////////////
	if (ExitReason == 0x00000012)
	{
		Log("VMCALL detected - CPU0", 0);

		if (GuestEAX == 0x12345678)
		{
			//	Switch off VMX mode.
			//
			Log("- Terminating VMX Mode.", 0xDEADDEAD);
			__asm
			{
				_emit	0x0F	// VMXOFF
					_emit	0x01
					_emit	0xC4
			}

			Log("- Flow Control Return to Address", GuestResumeEIP);

			__asm
			{
				POPAD
					MOV	ESP, GuestESP
					STI
					JMP	GuestResumeEIP
			}
		}

		Log("- Request has been denied.", ExitReason);

		__asm
		{
			POPAD
				JMP	Resume
		}
	}

	////////////
	//        //
	//  INVD  //
	//        //
	////////////
	if (ExitReason == 0x0000000C)
	{
		//Log( "INVD detected - CPU0" , 0 );

		__asm
		{
			_emit 0x0F
				_emit 0x08

				POPAD
				JMP		Resume
		}
	}

	/////////////
	//         //
	//  RDMSR  //
	//         //
	/////////////
	if (ExitReason == 0x0000001F)
	{
		Log("Read MSR - CPU0", GuestECX);
		__asm
		{
			POPAD

				MOV		ECX, GuestECX

				_emit	0x0F
				_emit	0x32

				JMP		Resume
		}
	}

	/////////////
	//         //
	//  WRMSR  //
	//         //
	/////////////
	if (ExitReason == 0x00000020)
	{
		Log("Write MSR - CPU0", GuestECX);
		__asm
		{
			POPAD

				MOV		ECX, GuestECX
				MOV		EAX, GuestEAX
				MOV		EDX, GuestEDX

				_emit	0x0F
				_emit	0x30

				JMP		Resume
		}
	}

	/////////////
	//         //
	//  CPUID  //
	//         //
	/////////////
	if (ExitReason == 0x0000000A)
	{
		if (HandlerLogging)
		{
			Log("CPUID detected - CPU0", 0);
			Log("- EAX", GuestEAX);
		}

		if (GuestEAX == 0x00000000)
		{
			__asm
			{
				POPAD

					MOV		EAX, 0x00000000

					CPUID

					MOV		EBX, 0x61656C43
					MOV		ECX, 0x2E636E6C
					MOV		EDX, 0x74614872

					JMP		Resume
			}
		}

		__asm
		{
			POPAD

				MOV		EAX, GuestEAX

				CPUID

				JMP		Resume
		}
	}

	///////////////////////////////
	//                           //
	//  Control Register Access  //
	//                           //
	///////////////////////////////
	if (ExitReason == 0x0000001C)
	{
		if (HandlerLogging) Log("Control Register Access detected.", 0);

		movcrControlRegister = (ExitQualification & 0x0000000F);
		movcrAccessType = ((ExitQualification & 0x00000030) >> 4);
		movcrOperandType = ((ExitQualification & 0x00000040) >> 6);
		movcrGeneralPurposeRegister = ((ExitQualification & 0x00000F00) >> 8);

		if (HandlerLogging)
		{
			Log("- movcrControlRegister", movcrControlRegister);
			Log("- movcrAccessType", movcrAccessType);
			Log("- movcrOperandType", movcrOperandType);
			Log("- movcrGeneralPurposeRegister", movcrGeneralPurposeRegister);
		}

		//	Control Register Access (CR3 <-- reg32)
		//
		if (movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 0)
		{
			WriteVMCS(0x00006802, GuestEAX);
			__asm POPAD
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 1)
		{
			WriteVMCS(0x00006802, GuestECX);
			__asm POPAD
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 2)
		{
			WriteVMCS(0x00006802, GuestEDX);
			__asm POPAD
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 3)
		{
			WriteVMCS(0x00006802, GuestEBX);
			__asm POPAD
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 4)
		{
			WriteVMCS(0x00006802, GuestESP);
			__asm POPAD
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 5)
		{
			WriteVMCS(0x00006802, GuestEBP);
			__asm POPAD
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 6)
		{
			WriteVMCS(0x00006802, GuestESI);
			__asm POPAD
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 0 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 7)
		{
			WriteVMCS(0x00006802, GuestEDI);
			__asm POPAD
			goto Resume;
		}
		//	Control Register Access (reg32 <-- CR3)
		//
		if (movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 0)
		{
			__asm	POPAD
			__asm	MOV EAX, GuestCR3
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 1)
		{
			__asm	POPAD
			__asm	MOV ECX, GuestCR3
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 2)
		{
			__asm	POPAD
			__asm	MOV EDX, GuestCR3
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 3)
		{
			__asm	POPAD
			__asm	MOV EBX, GuestCR3
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 4)
		{
			__asm	POPAD
			__asm	MOV ESP, GuestCR3
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 5)
		{
			__asm	POPAD
			__asm	MOV EBP, GuestCR3
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 6)
		{
			__asm	POPAD
			__asm	MOV ESI, GuestCR3
			goto Resume;
		}
		if (movcrControlRegister == 3 && movcrAccessType == 1 && movcrOperandType == 0 && movcrGeneralPurposeRegister == 7)
		{
			__asm	POPAD
			__asm	MOV EDI, GuestCR3
			goto Resume;
		}
	}


	//> somma
	// 우리은행 (touchenkey) 의 경우 이상한 동작 + 키보드 먹통 현상
	// outb 인스트럭션 발생 (GuestEAX = 0xD2)
	// --> 어떤 port 에 out 을 하는지 .. 로그 잘 찍어보기..
	// --> 다른 trap 또는 debugger 와 연관되어있는건 아닌가?
	//
	// 키보드가 먹통이 됨 (vmware 때문인지... ?)
	// IE 종료 후에도 계속 먹통
	if (ExitReason == EXIT_REASON_IO_INSTRUCTION)
	{
		Log("VM_EXIT with IO_BITMAP!!", 0xc0ffe);
		__asm int 3
		//
		//_vcpu.io_instr_exitq.value = ExitQualification;
		//if (0 == _vcpu.io_instr_exitq.operand_encoding)
		//{
		//	_vcpu.port = GuestEDX;
		//}
		//else
		//{
		//	_vcpu.port = _vcpu.io_instr_exitq.port_number;
		//}

		//_vcpu.io_size = _vcpu.io_instr_exitq.size_of_access + 1;	
		//
		//if (_vcpu.io_size != 1)
		//{
		//	Log( "oops! invalid _io_size=", _vcpu.io_size);
		//	__asm 
		//	{
		//		popad 
		//		jmp Resume;
		//	}
		//}

		//if (_vcpu.port != 0x60 && _vcpu.port != 0x64)
		//{
		//	Log( "oops! invalid port=", _vcpu.port);
		//	/*
		//	__asm 
		//	{
		//		popad 
		//		jmp Resume;
		//	}
		//	*/
		//}

		//if (0 == _vcpu.io_instr_exitq.direction)
		//{
		//	Log("emulate outb ", GuestEAX);

		//	// out - nothing to emulate
		//	// just resume
		//	__asm 
		//	{
		//		push	eax
		//		mov		eax, GuestEAX
		//		mov		edx, _vcpu.port
		//		out		dx, al
		//		pop		eax

		//		mov		eax, GuestEAX
		//		jmp		Resume
		//	}
		//}
		//else 
		//{
		//	// in
		//	
		//	if (_vcpu.port == 0x64)
		//	{
		//		// read status
		//		__asm
		//		{
		//			push	eax
		//			mov		edx, 0x64
		//			in		al, dx
		//			mov		_vcpu.port_status, al
		//			pop		eax
		//		}

		//		//> emulate 'inb'
		//		__asm 
		//		{
		//			popad
		//			mov		al, _vcpu.port_status
		//		}
		//		goto Resume;
		//	}
		//	else if (_vcpu.port == 0x60)
		//	{
		//		// read data
		//		__asm
		//		{
		//			push	eax
		//			mov		edx, 0x60
		//			in		al, dx
		//			mov		_vcpu.pc, al
		//			pop		eax
		//		}
		//		
		//		if (_vcpu.pc < 0x80) { KdPrint(("%c", scancode[_vcpu.pc]));}

		//		//> emulate 'inb'
		//		__asm 
		//		{
		//			popad
		//			mov		al, _vcpu.pc
		//		}
		//		goto Resume;
		//	}
		//	else
		//	{
		//		Log( "oops! invalid _vcpu.port=", _vcpu.port);
		//		__asm 
		//		{
		//			push	eax
		//			mov		edx, _vcpu.port
		//			in		al, dx
		//			mov		_vcpu.pc, al
		//			pop		eax

		//			popad
		//			mov		al, _vcpu.pc
		//			jmp		Resume
		//		}
		//	}
		//}		
	}

	Log("oops! no exit reason handler", ExitReason);
	__asm
	{
		popad
			jmp Resume;
	}
Exit:

	//
	//	Switch off VMX mode.
	//
	Log("Terminating VMX Mode.", 0xDEADDEAD);
	__asm
	{
		_emit	0x0F	// VMXOFF
			_emit	0x01
			_emit	0xC4
	}

	Log("Flow Control Return to Address", GuestEIP);

	__asm
	{
		POPAD
			MOV		ESP, GuestESP
			STI
			JMP		GuestEIP
	}

Resume:

	//	Need to execute the VMRESUME without having changed
	//	the state of the GPR and ESP et cetera.
	//
	__asm
	{
		STI

			_emit	0x0F	// VMRESUME
			_emit	0x01
			_emit	0xC3
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
**/
VOID SetBit(ULONG * dword, ULONG bit)
{
	ULONG mask = (1 << bit);
	*dword = *dword | mask;
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
VOID ClearBit(ULONG * dword, ULONG bit)
{
	ULONG mask = 0xFFFFFFFF;
	ULONG sub = (1 << bit);
	mask = mask - sub;
	*dword = *dword & mask;
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
void set_bit32(IN OUT ULONG* bit32_value, IN ULONG single_bit)
{
	if (single_bit > 31) return;

	ULONG mask = (1 << single_bit);
	*bit32_value = *bit32_value | mask;
}

/**
* @brief	Writes the contents of registers EDX:EAX into the 64-bit model specific
register (MSR) specified in the ECX register. The contents of the EDX
register are copied to high-order 32 bits of the selected MSR and the
contents of the EAX register are copied to low-order 32 bits of the MSR.
* @param
* @see
* @remarks
* @code
* @endcode
* @return
**/
VOID WriteVMCS(ULONG encoding, ULONG value)
{
	__asm
	{
		PUSHAD

			PUSH	value
			MOV		EAX, encoding

			_emit	0x0F				// VMWRITE EAX, [ESP]
			_emit	0x79
			_emit	0x04
			_emit	0x24

			POP EAX

			POPAD
	}
}

VOID WriteVMCS64(ULONG encoding, ULONGLONG value)
{
	__asm{
		pushad

			push value
			mov eax, encoding

			_emit 0x0f
			_emit 0x79
			_emit 0x04
			_emit 0x24

			pop eax

			popad


	}
}



/**
* @brief		Loads the contents of a 64-bit model specific register (MSR) specified
in the ECX register into registers EDX:EAX. The EDX register is loaded
with the high-order 32 bits of the MSR and the EAX register is loaded
with the low-order 32 bits.
msr.Hi --> EDX
msr.Lo --> EAX
* @param
* @see
* @remarks
* @code
* @endcode
* @return
**/
VOID ReadMSR(ULONG msrEncoding)
{
	__asm
	{
		PUSHAD

			MOV		ECX, msrEncoding

			RDMSR

			MOV		msr.Hi, EDX
			MOV		msr.Lo, EAX

			POPAD
	}
}

/**
* @brief	Write the msr data structure into MSR specified by msrEncoding.
msr.Hi <-- EDX
msr.Lo <-- EAX
* @param
* @see
* @remarks
* @code
* @endcode
* @return
**/
VOID WriteMSR(ULONG msrEncoding)
{
	__asm
	{
		PUSHAD

			MOV		EDX, msr.Hi
			MOV		EAX, msr.Lo
			MOV		ECX, msrEncoding

			WRMSR

			POPAD
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
**/
ULONG GetSegmentDescriptorBase(ULONG gdt_base, USHORT seg_selector)
{
	ULONG			base = 0;
	SEG_DESCRIPTOR	segDescriptor = { 0 };

	RtlCopyBytes(&segDescriptor, (ULONG *)(gdt_base + (seg_selector >> 3) * 8), 8);
	base = segDescriptor.BaseHi;
	base <<= 8;
	base |= segDescriptor.BaseMid;
	base <<= 16;
	base |= segDescriptor.BaseLo;

	return base;
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
ULONG GetSegmentDescriptorDPL(ULONG gdt_base, USHORT seg_selector)
{
	SEG_DESCRIPTOR	segDescriptor = { 0 };

	RtlCopyBytes(&segDescriptor, (ULONG *)(gdt_base + (seg_selector >> 3) * 8), 8);

	return segDescriptor.DPL;
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
ULONG GetSegmentDescriptorLimit(ULONG gdt_base, USHORT seg_selector)
{
	SEG_DESCRIPTOR	segDescriptor = { 0 };

	RtlCopyBytes(&segDescriptor, (ULONG *)(gdt_base + (seg_selector >> 3) * 8), 8);

	//return segDescriptor.LimitLo;
	return ((segDescriptor.LimitHi << 16) | segDescriptor.LimitLo);
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
NTSTATUS allocate_vcpu(OUT VCPU& cpu)
{
	NTSTATUS status = STATUS_SUCCESS;
	RtlZeroMemory(&cpu, sizeof(VCPU));

	do
	{
		//	Allocate the VMXON region memory.
		//	
		cpu.vmxon_region = (UCHAR*)MmAllocateNonCachedMemory(VMXON_RGN_SIZE);
		if (NULL == cpu.vmxon_region)
		{
			log_err "MmAllocateNonCachedMemory( vmxon_region )" log_end
				status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlZeroMemory(cpu.vmxon_region, VMXON_RGN_SIZE);
		cpu.vmxon_region_physical = (PHYSICAL_ADDRESS)MmGetPhysicalAddress(cpu.vmxon_region);

		//	Allocate the VMCS region memory.
		//
		cpu.vmcs_region = (UCHAR*)MmAllocateNonCachedMemory(VMCS_RGN_SIZE);
		if (NULL == cpu.vmcs_region)
		{
			log_err "MmAllocateNonCachedMemory( vmcs_region )" log_end
				status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlZeroMemory(cpu.vmcs_region, VMCS_RGN_SIZE);
		cpu.vmcs_region_physical = (PHYSICAL_ADDRESS)MmGetPhysicalAddress(cpu.vmcs_region);

		// allocate I/O bitmap A memory
		cpu.io_bitmap_a = (UCHAR*)MmAllocateNonCachedMemory(VMX_RGN_BLOCKSIZE);
		if (NULL == cpu.io_bitmap_a)
		{
			log_err "MmAllocateNonCachedMemory( io_bitmap_a )" log_end
				status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlZeroMemory((void*)cpu.io_bitmap_a, VMX_RGN_BLOCKSIZE);
		cpu.io_bitmap_a_physical = MmGetPhysicalAddress((void*)cpu.io_bitmap_a);

		// allocate I/O bitmap B memory
		cpu.io_bitmap_b = (UCHAR*)MmAllocateNonCachedMemory(VMX_RGN_BLOCKSIZE);
		if (NULL == cpu.io_bitmap_b)
		{
			log_err "MmAllocateNonCachedMemory( io_bitmap_b )" log_end
				status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlZeroMemory((void*)cpu.io_bitmap_b, VMX_RGN_BLOCKSIZE);
		cpu.io_bitmap_b_physical = MmGetPhysicalAddress((void*)cpu.io_bitmap_b);

		/* 
			investigate MTRR register
		*/
		RtlZeroMemory(ranges, 0, MAX_SUPPORTED_MTRR_RANGE*sizeof(MTRR_RANGE));
		RtlZeroMemory(fixed_ranges, 0, MAX_SUPPORTED_MTRR_RANGE*sizeof(MTRR_FIXED_RANGE));

		ReadMSR(0xfe);
		int count = ((((unsigned long long) msr.Hi) << 32) | msr.Lo) & 0xff;
		int i;
		for(i = 0; i < count; i++) {
			MSR base, mask;
			ReadMSR( 0x200 + (i<<1));//, &base);
			base = msr;
			ReadMSR( 0x201 + (i<<1));//, &mask);
			mask = msr;
			if(i >= MAX_SUPPORTED_MTRR_RANGE) {
				__asm int 3
			}
			if(mask.Lo & (1<<11) ) {
				ranges[i].base = base.Lo & 0xfffff000;
				ranges[i].size = (~(mask.Lo & 0xfffff000)) & 0xfffff000;
				ranges[i].type = (unsigned char) (base.Lo & 0xff);
			}
		}

		i = 0;
		ReadMSR(0x250 );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x258 );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x259 );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x268 );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x269 );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x26a );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x26b );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x26c );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x26d );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x26e );
		fixed_ranges[i++].types = msr.Lo;
		ReadMSR(0x26f );
		fixed_ranges[i++].types = msr.Lo;
	

		/*
			END
		*/

		/*
		Allocate EPT Memory
		*/
		cpu.ept = (PEPTPML4)MmAllocateNonCachedMemory(sizeof(EPTPML4));
		cpu.ept_physical = MmGetPhysicalAddress((void*)cpu.ept);
		RtlZeroMemory((void*)cpu.ept, sizeof(EPTPML4));


		cpu.ept_pdpte = (PEPTPDPT)MmAllocateNonCachedMemory(sizeof(EPTPDPT));
		cpu.ept_pdpte_physical = MmGetPhysicalAddress((void*)cpu.ept_pdpte);
		RtlZeroMemory((void*)cpu.ept_pdpte, sizeof(EPTPDPT));

		for( int i = 0 ; i < 4; i ++){
			cpu.ept_pd[i] = (PEPTPD)MmAllocateNonCachedMemory(sizeof(EPTPD));
			cpu.ept_pd_physical[i] = MmGetPhysicalAddress((void*)cpu.ept_pd[i]);
			RtlZeroMemory((void*)cpu.ept_pd[i], sizeof(EPTPD));	
		}

		Log("EPT Address LowPart", cpu.ept_physical.LowPart);
		Log("EPT Address HighPart", cpu.ept_physical.HighPart);

		Log("EPT PDPTE LowPart", cpu.ept_pdpte_physical.LowPart);
		Log("EPT PDPTE HighPart", cpu.ept_pdpte_physical.HighPart);

		cpu.ept->a[0].u = cpu.ept_pdpte_physical.QuadPart | 0x7; /* READ WRITE EXECUTE */
		//cpu.ept->a[1].u = cpu.ept_pdpte_physical.QuadPart | 0x7; /* READ WRITE EXECUTE */
		//cpu.ept->a[2].u = cpu.ept_pdpte_physical.QuadPart | 0x7; /* READ WRITE EXECUTE */
		//cpu.ept->a[3].u = cpu.ept_pdpte_physical.QuadPart | 0x7; /* READ WRITE EXECUTE */
		
		ULONGLONG m = 0;
		for(int o = 0 ; o < 4; o++ ){
			cpu.ept_pdpte->a[o].u = cpu.ept_pd_physical[o].QuadPart | 0x7; /* READ WRITE EXECUTE */

			for (int i = 0; i < 512; i++)
			{
				PEPTPT eptpt = (PEPTPT) MmAllocateNonCachedMemory(sizeof(EPTPT));
				RtlZeroMemory((void*)eptpt, sizeof(EPTPT));
				PHYSICAL_ADDRESS new_addr = MmGetPhysicalAddress((void*)eptpt);
				//cpu.ept_pd->a[i].u = m | 0x7 | (3 << 3) /*mem type WB*/| (1 << 7) /* mem mapping 2m */ | (1 << 6)/* ignore pat mem type */;
				// not useing 2M but 4K
				cpu.ept_pd[o]->a[i].u = new_addr.QuadPart | 0x7;
				for (int k = 0; k < 512; k++)
				{
					eptpt->a[k].u = m  | ((EPTGetMemoryType(m)) << 3) | 0x7 /*mem type WB*/;
					m += 4096;
				}
			}
		}
		Log("Allocate Last Mem", m);

		/*
		Allocate EPT Memory ENDs
		*/


		//	Allocate stack for the VM Exit Handler.
		cpu.fake_stack = (UCHAR*)ExAllocatePoolWithTag(NonPagedPool, FAKE_STACK_SIZE, FAKE_STACK_TAG);
		if (cpu.fake_stack == NULL)
		{
			log_err "ExAllocatePoolWithTag( fake_stack )" log_end
				status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
	} while (false);


	if (TRUE != NT_SUCCESS(status))
	{
		free_vcpu(cpu);
	}
	else
	{
		log_info
			"vmxon region [v] = 0x%p, vmxon region [p] = 0x%p",
			cpu.vmxon_region,
			cpu.vmxon_region_physical
			log_end

			log_info
			"vmcs  region [v] = 0x%p, vmxon region [p] = 0x%p",
			cpu.vmcs_region,
			cpu.vmcs_region_physical
			log_end

			log_info
			"io_bitmap_a  [v] = 0x%p, io_bitmap_a  [p] = 0x%p",
			cpu.io_bitmap_a,
			cpu.io_bitmap_a_physical
			log_end

			log_info
			"io_bitmap_b  [v] = 0x%p, io_bitmap_b  [p] = 0x%p",
			cpu.io_bitmap_b,
			cpu.io_bitmap_b_physical
			log_end

			log_info "fake stack = 0x%p", cpu.fake_stack log_end
	}

	return status;
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
void free_vcpu(IN OUT VCPU& cpu)
{
	if (NULL != cpu.vmxon_region)
	{
		MmFreeNonCachedMemory(cpu.vmxon_region, VMXON_RGN_SIZE);
		cpu.vmxon_region = NULL;
	}

	if (NULL != cpu.vmcs_region)
	{
		MmFreeNonCachedMemory(cpu.vmcs_region, VMCS_RGN_SIZE);
		cpu.vmcs_region = NULL;
	}

	if (NULL != cpu.io_bitmap_a)
	{
		MmFreeNonCachedMemory(cpu.io_bitmap_a, VMX_RGN_BLOCKSIZE);
		cpu.io_bitmap_a = NULL;
	}

	if (NULL != cpu.io_bitmap_b)
	{
		MmFreeNonCachedMemory(cpu.io_bitmap_b, VMX_RGN_BLOCKSIZE);
		cpu.io_bitmap_b = NULL;
	}

	if (NULL != cpu.fake_stack)
	{
		ExFreePoolWithTag(cpu.fake_stack, FAKE_STACK_TAG);
		cpu.fake_stack = NULL;
	}
}