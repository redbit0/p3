#ifndef _MEM_H
#define _MEM_H

//This is the virtual base address for the page directory of the current process

#define VIRTUAL_PD_BASE     0xC0300000 
#define VIRTUAL_PD_BASE_PAE 0xC0600000 /* Use this when PAE is enabled */
#define VIRTUAL_PT_BASE     0xC0000000 

//> Page table entry
typedef struct _PTE
{
  ULONG Present           :1;
  ULONG Writable          :1;
  ULONG Owner             :1;
  ULONG WriteThrough      :1;
  ULONG CacheDisable      :1;
  ULONG Accessed          :1;
  ULONG Dirty             :1;
  ULONG LargePage         :1;
  ULONG Global            :1;
  ULONG ForUse1           :1;
  ULONG ForUse2           :1;
  ULONG ForUse3           :1;
  ULONG PageBaseAddr      :20;
  ULONG NoUse			  :32;
} PTE, *PPTE;

/* Memory-management defines */
#define PAGE_SIZE 0x1000
#define LARGEPAGE_SIZE 0x400000
#define LARGEPAGE_SIZE 0X200000

#define CR3_TO_PDBASE(v) ((v) & 0xfffff000)
#define CR3_TO_PDPTBASE_PAE(v) ((v) & 0xfffffff0)

//#define PDPTE_TO_PDBASE(v) ((v) & 0xfffff000)

#define VA_TO_PDE(a)   (((a) & 0xffc00000) >> 22)
#define VA_TO_PTE(a)   (((a) & 0x003ff000) >> 12)

#define VA_TO_PDPTE(a)		(((a) & 0xC0000000) >> 30)
#define VA_TO_PDE_PAE(a)	(((a) & 0x3fe00000) >> 21)
#define VA_TO_PDE_PAE_WIN(a) (((a) & 0xffe00000) >> 21)
#define VA_TO_PTE_PAE(a)	(((a) & 0x001ff000) >> 12)

#define PHY_TO_FRAME(a) ((a) >> 12)
#define FRAME_TO_PHY(a) ((a) << 12)

#define PHY_TO_LARGEFRAME(a) ((a) >> 22)
#define LARGEFRAME_TO_PHY(a) ((a) << 22)

#define PHY_TO_LARGEFRAME_PAE(a) ((a) >> 21)
#define LARGEFRAME_TO_PHY_PAE(a) ((a) << 21)

#define PDE_TO_VALID(a)  ((ULONG32) (a) & 0x1)

#define PAGE_ALIGN(a) ((a) & ~((PAGE_SIZE)-1))
#define LARGEPAGE_ALIGN(a) ((a) & ~((LARGEPAGE_SIZE)-1))

#define PAGE_OFFSET(a) ((ULONG) (a) - (ULONG) PAGE_ALIGN(a))
#define LARGEPAGE_OFFSET(a) ((ULONG) (a) - (ULONG) LARGEPAGE_ALIGN(a))

NTSTATUS MmuMapPhysicalPage(ULONG phy, PULONG pva, PPTE poriginal);
NTSTATUS MmuUnmapPhysicalPage(ULONG va, PTE original);

#define MmuWritePhysicalRegion(phy, buffer, size) MmuReadWritePhysicalRegion(phy, buffer, size, TRUE)
#define MmuReadPhysicalRegion(phy, buffer, size)  MmuReadWritePhysicalRegion(phy, buffer, size, FALSE)

NTSTATUS MmuReadWritePhysicalRegion(ULONG phy, PVOID buffer, ULONG size, BOOLEAN isWrite);

NTSTATUS MmuGetPageEntryPAE(ULONG cr3, ULONG va, PPTE ppte, PBOOLEAN pisLargePage);
NTSTATUS MmuGetPhysicalAddress(ULONG cr3, ULONG va, PULONG pphy);
BOOLEAN  MmuIsAddressValid(ULONG cr3, ULONG va);
BOOLEAN  MmuIsAddressWritable(ULONG cr3, ULONG va);

NTSTATUS MmuMapPhysicalPage(ULONG phy, PULONG pva, PPTE pentryOriginal);
NTSTATUS MmuUnmapPhysicalPage(ULONG va, PTE entryOriginal);
VOID MmuInvalidateTLB(ULONG addr);
NTSTATUS MmuFindUnusedPTEUsePAE(PULONG pdwLogical);

#endif	/* _MMU_H */