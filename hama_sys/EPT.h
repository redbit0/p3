/** @name VMX EPT paging structures
* @{
*/
/** @name MSR_IA32_VMX_EPT_VPID_CAPS; EPT capabilities MSR
* @{
*/

typedef struct _MTRR_FIXED_RANGE {
  __int32 types;
} MTRR_FIXED_RANGE, *PMTRR_FIXED_RANGE;

typedef struct _MTRR_RANGE {
  unsigned __int32 base;
  unsigned __int64 size;
  unsigned char type;
  
} MTRR_RANGE, *PMTRR_RANGE;

#define MAX_SUPPORTED_MTRR_RANGE 32
#define MAX_SUPPORTED_MTRR_FIXED_RANGE 12





#define RT_BIT_64(bit)                          ( UINT64_C(1) << (bit) )

#define IA32_VMX_EPT_VPID_CAP					0x48c

#define MSR_IA32_VMX_EPT_VPID_CAP_RWX_X_ONLY                             RT_BIT_64(0)
#define MSR_IA32_VMX_EPT_VPID_CAP_RWX_W_ONLY                             RT_BIT_64(1)
#define MSR_IA32_VMX_EPT_VPID_CAP_RWX_WX_ONLY                            RT_BIT_64(2)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_21_BITS                            RT_BIT_64(3)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_30_BITS                            RT_BIT_64(4)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_39_BITS                            RT_BIT_64(5)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_48_BITS                            RT_BIT_64(6)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_57_BITS                            RT_BIT_64(7)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_UC                                 RT_BIT_64(8)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WC                                 RT_BIT_64(9)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WT                                 RT_BIT_64(12)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WP                                 RT_BIT_64(13)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WB                                 RT_BIT_64(14)
#define MSR_IA32_VMX_EPT_VPID_CAP_SP_21_BITS                             RT_BIT_64(16)
#define MSR_IA32_VMX_EPT_VPID_CAP_SP_30_BITS                             RT_BIT_64(17)
#define MSR_IA32_VMX_EPT_VPID_CAP_SP_39_BITS                             RT_BIT_64(18)
#define MSR_IA32_VMX_EPT_VPID_CAP_SP_48_BITS                             RT_BIT_64(19)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT                                 RT_BIT_64(20)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_SINGLE_CONTEXT                  RT_BIT_64(25)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS                    RT_BIT_64(26)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID                                RT_BIT_64(32)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR                     RT_BIT_64(40)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT                 RT_BIT_64(41)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_ALL_CONTEXTS                   RT_BIT_64(42)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS  RT_BIT_64(43)

#define VMX_EXIT_EPT_VIOLATION                                  48
/** 49 EPT misconfiguration. An attempt to access memory with a guest-physical address encountered a misconfigured EPT paging-structure entry. */
#define VMX_EXIT_EPT_MISCONFIG                                  49
/** 50 INVEPT. Guest software attempted to execute INVEPT. */

#define X86_PG_PAE_ENTRIES                  512
/** Number of entries in a PAE PDPT. */
#define X86_PG_PAE_PDPE_ENTRIES             4

/**
* Number of page table entries in the EPT. (PDPTE/PDE/PTE)
*/
#define EPT_PG_ENTRIES          X86_PG_PAE_ENTRIES

typedef __int32 uint32_t;
typedef __int64 uint64_t;
typedef uint64_t X86PGPAEUINT;

#define AssertCompileSize(a,b)
/**
* EPT Page Directory Pointer Entry. Bit view.
* @todo uint64_t isn't safe for bitfields (gcc pedantic warnings, and IIRC,
*       this did cause trouble with one compiler/version).
*/
#pragma pack(1)
typedef struct EPTPML4EBITS
{
	/** Present bit. */
	uint64_t    u1Present : 1;
	/** Writable bit. */
	uint64_t    u1Write : 1;
	/** Executable bit. */
	uint64_t    u1Execute : 1;
	/** Reserved (must be 0). */
	uint64_t    u5Reserved : 5;
	/** Available for software. */
	uint64_t    u4Available : 4;
	/** Physical address of the next level (PD). Restricted by maximum physical address width of the cpu. */
	uint64_t    u40PhysAddr : 40;
	/** Availabe for software. */
	uint64_t    u12Available : 12;
} EPTPML4EBITS;
#pragma pack()
AssertCompileSize(EPTPML4EBITS, 8);

/** Bits 12-51 - - EPT - Physical Page number of the next level. */
#define EPT_PML4E_PG_MASK       X86_PML4E_PG_MASK
/** The page shift to get the PML4 index. */
#define EPT_PML4_SHIFT          X86_PML4_SHIFT
/** The PML4 index mask (apply to a shifted page address). */
#define EPT_PML4_MASK           X86_PML4_MASK

/**
* EPT PML4E.
*/
#pragma pack(1)
typedef union EPTPML4E
{
	/** Normal view. */
	EPTPML4EBITS    n;
	/** Unsigned integer view. */
	X86PGPAEUINT    u;
	/** 64 bit unsigned integer view. */
	uint64_t        au64[1];
	/** 32 bit unsigned integer view. */
	uint32_t        au32[2];
} EPTPML4E;
#pragma pack()
/** Pointer to a PML4 table entry. */
typedef EPTPML4E *PEPTPML4E;
/** Pointer to a const PML4 table entry. */
typedef const EPTPML4E *PCEPTPML4E;
AssertCompileSize(EPTPML4E, 8);

/**
* EPT PML4 Table.
*/
#pragma pack(1)
typedef struct EPTPML4
{
	EPTPML4E    a[EPT_PG_ENTRIES];
} EPTPML4;
#pragma pack()
/** Pointer to an EPT PML4 Table. */
typedef EPTPML4 *PEPTPML4;
/** Pointer to a const EPT PML4 Table. */
typedef const EPTPML4 *PCEPTPML4;

/**
* EPT Page Directory Pointer Entry. Bit view.
*/
#pragma pack(1)
typedef struct EPTPDPTEBITS
{
	/** Present bit. */
	uint64_t    u1Present : 1;
	/** Writable bit. */
	uint64_t    u1Write : 1;
	/** Executable bit. */
	uint64_t    u1Execute : 1;
	/** Reserved (must be 0). */
	uint64_t    u5Reserved : 5;
	/** Available for software. */
	uint64_t    u4Available : 4;
	/** Physical address of the next level (PD). Restricted by maximum physical address width of the cpu. */
	uint64_t    u40PhysAddr : 40;
	/** Availabe for software. */
	uint64_t    u12Available : 12;
} EPTPDPTEBITS;
#pragma pack()
AssertCompileSize(EPTPDPTEBITS, 8);

/** Bits 12-51 - - EPT - Physical Page number of the next level. */
#define EPT_PDPTE_PG_MASK       X86_PDPE_PG_MASK
/** The page shift to get the PDPT index. */
#define EPT_PDPT_SHIFT          X86_PDPT_SHIFT
/** The PDPT index mask (apply to a shifted page address). */
#define EPT_PDPT_MASK           X86_PDPT_MASK_AMD64

/**
* EPT Page Directory Pointer.
*/
#pragma pack(1)
typedef union EPTPDPTE
{
	/** Normal view. */
	EPTPDPTEBITS    n;
	/** Unsigned integer view. */
	X86PGPAEUINT    u;
	/** 64 bit unsigned integer view. */
	uint64_t        au64[1];
	/** 32 bit unsigned integer view. */
	uint32_t        au32[2];
} EPTPDPTE;
#pragma pack()
/** Pointer to an EPT Page Directory Pointer Entry. */
typedef EPTPDPTE *PEPTPDPTE;
/** Pointer to a const EPT Page Directory Pointer Entry. */
typedef const EPTPDPTE *PCEPTPDPTE;
AssertCompileSize(EPTPDPTE, 8);

/**
* EPT Page Directory Pointer Table.
*/
#pragma pack(1)
typedef struct EPTPDPT
{
	EPTPDPTE    a[EPT_PG_ENTRIES];
} EPTPDPT;
#pragma pack()
/** Pointer to an EPT Page Directory Pointer Table. */
typedef EPTPDPT *PEPTPDPT;
/** Pointer to a const EPT Page Directory Pointer Table. */
typedef const EPTPDPT *PCEPTPDPT;


/**
* EPT Page Directory Table Entry. Bit view.
*/
#pragma pack(1)
typedef struct EPTPDEBITS
{
	/** Present bit. */
	uint64_t    u1Present : 1;
	/** Writable bit. */
	uint64_t    u1Write : 1;
	/** Executable bit. */
	uint64_t    u1Execute : 1;
	/** Reserved (must be 0). */
	uint64_t    u4Reserved : 4;
	/** Big page (must be 0 here). */
	uint64_t    u1Size : 1;
	/** Available for software. */
	uint64_t    u4Available : 4;
	/** Physical address of page table. Restricted by maximum physical address width of the cpu. */
	uint64_t    u40PhysAddr : 40;
	/** Availabe for software. */
	uint64_t    u12Available : 12;
} EPTPDEBITS;
#pragma pack()
AssertCompileSize(EPTPDEBITS, 8);

/** Bits 12-51 - - EPT - Physical Page number of the next level. */
#define EPT_PDE_PG_MASK         X86_PDE_PAE_PG_MASK
/** The page shift to get the PD index. */
#define EPT_PD_SHIFT            X86_PD_PAE_SHIFT
/** The PD index mask (apply to a shifted page address). */
#define EPT_PD_MASK             X86_PD_PAE_MASK

/**
* EPT 2MB Page Directory Table Entry. Bit view.
*/
#pragma pack(1)
typedef struct EPTPDE2MBITS
{
	/** Present bit. */
	uint64_t    u1Present : 1;
	/** Writable bit. */
	uint64_t    u1Write : 1;
	/** Executable bit. */
	uint64_t    u1Execute : 1;
	/** EPT Table Memory Type. MBZ for non-leaf nodes. */
	uint64_t    u3EMT : 3;
	/** Ignore PAT memory type */
	uint64_t    u1IgnorePAT : 1;
	/** Big page (must be 1 here). */
	uint64_t    u1Size : 1;
	/** Available for software. */
	uint64_t    u4Available : 4;
	/** Reserved (must be 0). */
	uint64_t    u9Reserved : 9;
	/** Physical address of the 2MB page. Restricted by maximum physical address width of the cpu. */
	uint64_t    u31PhysAddr : 31;
	/** Availabe for software. */
	uint64_t    u12Available : 12;
} EPTPDE2MBITS;
#pragma pack()
AssertCompileSize(EPTPDE2MBITS, 8);

/** Bits 21-51 - - EPT - Physical Page number of the next level. */
#define EPT_PDE2M_PG_MASK       X86_PDE2M_PAE_PG_MASK

/**
* EPT Page Directory Table Entry.
*/
#pragma pack(1)
typedef union EPTPDE
{
	/** Normal view. */
	EPTPDEBITS      n;
	/** 2MB view (big). */
	EPTPDE2MBITS    b;
	/** Unsigned integer view. */
	X86PGPAEUINT    u;
	/** 64 bit unsigned integer view. */
	uint64_t        au64[1];
	/** 32 bit unsigned integer view. */
	uint32_t        au32[2];
} EPTPDE;
#pragma pack()
/** Pointer to an EPT Page Directory Table Entry. */
typedef EPTPDE *PEPTPDE;
/** Pointer to a const EPT Page Directory Table Entry. */
typedef const EPTPDE *PCEPTPDE;
AssertCompileSize(EPTPDE, 8);

/**
* EPT Page Directory Table.
*/
#pragma pack(1)
typedef struct EPTPD
{
	EPTPDE      a[EPT_PG_ENTRIES];
} EPTPD;
#pragma pack()
/** Pointer to an EPT Page Directory Table. */
typedef EPTPD *PEPTPD;
/** Pointer to a const EPT Page Directory Table. */
typedef const EPTPD *PCEPTPD;


/**
* EPT Page Table Entry. Bit view.
*/
#pragma pack(1)
typedef struct EPTPTEBITS
{
	/** 0 - Present bit.
	* @remark This is a convenience "misnomer".  The bit actually indicates
	*         read access and the CPU will consider an entry with any of the
	*         first three bits set as present.  Since all our valid entries
	*         will have this bit set, it can be used as a present indicator
	*         and allow some code sharing. */
	uint64_t    u1Present : 1;
	/** 1 - Writable bit. */
	uint64_t    u1Write : 1;
	/** 2 - Executable bit. */
	uint64_t    u1Execute : 1;
	/** 5:3 - EPT Memory Type. MBZ for non-leaf nodes. */
	uint64_t    u3EMT : 3;
	/** 6 - Ignore PAT memory type */
	uint64_t    u1IgnorePAT : 1;
	/** 11:7 - Available for software. */
	uint64_t    u5Available : 5;
	/** 51:12 - Physical address of page. Restricted by maximum physical
	*  address width of the cpu. */
	uint64_t    u40PhysAddr : 40;
	/** 63:52 - Available for software. */
	uint64_t    u12Available : 12;
} EPTPTEBITS;
#pragma pack()
AssertCompileSize(EPTPTEBITS, 8);

/** Bits 12-51 - - EPT - Physical Page number of the next level. */
#define EPT_PTE_PG_MASK         X86_PTE_PAE_PG_MASK
/** The page shift to get the EPT PTE index. */
#define EPT_PT_SHIFT            X86_PT_PAE_SHIFT
/** The EPT PT index mask (apply to a shifted page address). */
#define EPT_PT_MASK             X86_PT_PAE_MASK

/**
* EPT Page Table Entry.
*/
#pragma pack(1)
typedef union EPTPTE
{
	/** Normal view. */
	EPTPTEBITS      n;
	/** Unsigned integer view. */
	X86PGPAEUINT    u;
	/** 64 bit unsigned integer view. */
	uint64_t        au64[1];
	/** 32 bit unsigned integer view. */
	uint32_t        au32[2];
} EPTPTE;
#pragma pack()
/** Pointer to an EPT Page Directory Table Entry. */
typedef EPTPTE *PEPTPTE;
/** Pointer to a const EPT Page Directory Table Entry. */
typedef const EPTPTE *PCEPTPTE;
AssertCompileSize(EPTPTE, 8);

/**
* EPT Page Table.
*/
#pragma pack(1)
typedef struct EPTPT
{
	EPTPTE      a[EPT_PG_ENTRIES];
} EPTPT;
#pragma pack()
/** Pointer to an extended page table. */
typedef EPTPT *PEPTPT;
/** Pointer to a const extended table. */
typedef const EPTPT *PCEPTPT;