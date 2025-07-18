#include <dolphin.h>
#include <dolphin/os.h>
#include <dolphin/os/OSLink.h>

#include "os/__os.h"

#define SHN_UNDEF 0
#define SHN_LORESERVE 0xff00
#define SHN_LOPROC 0xff00
#define SHN_HIPROC 0xff1f
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2
#define SHN_HIRESERVE 0xffff

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

//      Name                    Value       Field   Calculation
#define R_PPC_NONE 0             //  none    none
#define R_PPC_ADDR32 1           //  word32  S + A
#define R_PPC_ADDR24 2           //  low24*  (S + A) >> 2
#define R_PPC_ADDR16 3           //  half16* S + A
#define R_PPC_ADDR16_LO 4        //  half16  #lo(S + A)
#define R_PPC_ADDR16_HI 5        //  half16  #hi(S + A)
#define R_PPC_ADDR16_HA 6        //  half16  #ha(S + A)
#define R_PPC_ADDR14 7           //  low14*  (S + A) >> 2
#define R_PPC_ADDR14_BRTAKEN 8   //  low14*  (S + A) >> 2
#define R_PPC_ADDR14_BRNTAKEN 9  //  low14*  (S + A) >> 2
#define R_PPC_REL24 10           //  low24*  (S + A - P) >> 2
#define R_PPC_REL14 11           //  low14*  (S + A - P) >> 2
#define R_PPC_REL14_BRTAKEN 12   //  low14*  (S + A - P) >> 2
#define R_PPC_REL14_BRNTAKEN 13  //  low14*  (S + A - P) >> 2

#define R_PPC_GOT16 14     //  half16* G + A
#define R_PPC_GOT16_LO 15  //  half16  #lo(G + A)
#define R_PPC_GOT16_HI 16  //  half16  #hi(G + A)
#define R_PPC_GOT16_HA 17  //  half16  #ha(G + A)
#define R_PPC_PLTREL24 18  //  low24*  (L + A - P) >> 2
#define R_PPC_COPY 19      //  none    none
#define R_PPC_GLOB_DAT 20  //  word32  S + A
#define R_PPC_JMP_SLOT 21  //  none
#define R_PPC_RELATIVE 22  //  word32  B + A

#define R_PPC_LOCAL24PC 23  //  low24*

#define R_PPC_UADDR32 24  //  word32  S + A
#define R_PPC_UADDR16 25  //  half16* S + A
#define R_PPC_REL32 26    //  word32  S + A - P

#define R_PPC_PLT32 27     //  word32  L + A
#define R_PPC_PLTREL32 28  //  word32  L + A - P
#define R_PPC_PLT16_LO 29  //  half16  #lo(L + A)
#define R_PPL_PLT16_HI 30  //  half16  #hi(L + A)
#define R_PPC_PLT16_HA 31  //  half16  #ha(L + A)

#define R_PPC_SDAREL16 32    //  half16* S + A - _SDA_BASE_
#define R_PPC_SECTOFF 33     //  half16* R + A
#define R_PPC_SECTOFF_LO 34  //  half16  #lo(R + A)
#define R_PPC_SECTOFF_HI 35  //  half16  #hi(R + A)
#define R_PPC_SECTOFF_HA 36  //  half16  #ha(R + A)
#define R_PPC_ADDR30 37      //  word30  (S + A - P) >> 2

#define R_PPC_EMB_NADDR32 101     //  uword32 N       (A - S)
#define R_PPC_EMB_NADDR16 102     //  uhalf16 Y       (A - S)
#define R_PPC_EMB_NADDR16_LO 103  //  uhalf16 N       #lo(A - S)
#define R_PPC_EMB_NADDR16_HI 104  //  uhalf16 N       #hi(A - S)
#define R_PPC_EMB_NADDR16_HA 105  //  uhalf16 N       #ha(A - S)
#define R_PPC_EMB_SDAI16 106      //  uhalf16 Y       T
#define R_PPC_EMB_SDA2I16 107     //  uhalf16 Y       U
#define R_PPC_EMB_SDA2REL 108     //  uhalf16 Y       S + A - _SDA2_BASE_
#define R_PPC_EMB_SDA21 109       //  ulow21  N
#define R_PPC_EMB_MRKREF 110      //  none    N
#define R_PPC_EMB_RELSEC16 111    //  uhalf16 Y       V + A
#define R_PPC_EMB_RELST_LO 112    //  uhalf16 N       #lo(W + A)
#define R_PPC_EMB_RELST_HI 113    //  uhalf16 N       #hi(W + A)
#define R_PPC_EMB_RELST_HA 114    //  uhalf16 N       #ha(W + A)
#define R_PPC_EMB_BIT_FLD 115     //  uword32 Y
#define R_PPC_EMB_RELSDA 116      //  uhalf16 Y

#define ENQUEUE_INFO(queue, info, link) \
  do {                                  \
    OSModuleInfo* __prev;               \
                                        \
    __prev = (queue)->tail;             \
    if (__prev == NULL)                 \
      (queue)->head = (info);           \
    else                                \
      __prev->link.next = (info);       \
    (info)->link.prev = __prev;         \
    (info)->link.next = NULL;           \
    (queue)->tail = (info);             \
  } while (0)

#define DEQUEUE_INFO(info, queue, link) \
  do {                                  \
    OSModuleInfo* __next;               \
    OSModuleInfo* __prev;               \
                                        \
    __next = (info)->link.next;         \
    __prev = (info)->link.prev;         \
                                        \
    if (__next == NULL)                 \
      (queue)->tail = __prev;           \
    else                                \
      __next->link.prev = __prev;       \
                                        \
    if (__prev == NULL)                 \
      (queue)->head = __next;           \
    else                                \
      __prev->link.next = __next;       \
  } while (0)

OSModuleQueue __OSModuleInfoList AT_ADDRESS(OS_BASE_CACHED | 0x30C8);

#pragma dont_inline on

__declspec(weak) void OSNotifyLink(void) {}
__declspec(weak) void OSNotifyUnlink() {}

#pragma dont_inline reset

void OSSetStringTable(const void* string_table) {
    __OSStringTable = string_table;
}

static BOOL Relocate(OSModuleHeader* newModule, OSModuleHeader* module) {
    u32 idNew;
    OSImportInfo* imp;
    OSRel* rel;
    OSSectionInfo* si;
    OSSectionInfo* siFlush;
    u32* p;
    u32 offset;
    u32 x;

    idNew = newModule ? newModule->info.id : 0;
    for (imp = (OSImportInfo*)module->impOffset;
         imp < (OSImportInfo*)(module->impOffset + module->impSize); imp++)
    {
        if (imp->id == idNew) {
            goto Found;
        }
    }
    return FALSE;

Found:
    siFlush = 0;
    for (rel = (OSRel*)imp->offset; rel->type != R_DOLPHIN_END; rel++) {
        (u8*)p += rel->offset;
        if (idNew) {
            si = &OSGetSectionInfo(newModule)[rel->section];
            offset = OS_SECTIONINFO_OFFSET(si->offset);
        } else {
            offset = 0;
        }
        switch (rel->type) {
        case R_PPC_NONE:
            break;
        case R_PPC_ADDR32:
            x = offset + rel->addend;
            *p = x;
            break;
        case R_PPC_ADDR24:
            x = offset + rel->addend;
            *p = (*p & ~0x03fffffc) | (x & 0x03fffffc);
            break;
        case R_PPC_ADDR16:
            x = offset + rel->addend;
            *(u16*)p = (u16)(x & 0xffff);
            break;
        case R_PPC_ADDR16_LO:
            x = offset + rel->addend;
            *(u16*)p = (u16)(x & 0xffff);
            break;
        case R_PPC_ADDR16_HI:
            x = offset + rel->addend;
            *(u16*)p = (u16)(((x >> 16) & 0xffff));
            break;
        case R_PPC_ADDR16_HA:
            x = offset + rel->addend;
            *(u16*)p = (u16)(((x >> 16) + ((x & 0x8000) ? 1 : 0)) & 0xffff);
            break;
        case R_PPC_REL24:
            x = offset + rel->addend - (u32)p;
            *p = (*p & ~0x03fffffc) | (x & 0x03fffffc);
            break;
        case R_DOLPHIN_NOP:
            break;
        case R_DOLPHIN_SECTION:
            si = &OSGetSectionInfo(module)[rel->section];
            p = (u32*)OS_SECTIONINFO_OFFSET(si->offset);
            if (siFlush) {
                offset = OS_SECTIONINFO_OFFSET(siFlush->offset);
                DCFlushRange((void*)offset, siFlush->size);
                ICInvalidateRange((void*)offset, siFlush->size);
            }
            siFlush = (si->offset & OS_SECTIONINFO_EXEC) ? si : 0;
            break;
        default:
            OSReport("OSLink: unknown relocation type %3d\n", rel->type);
            break;
        }
    }

    if (siFlush) {
        offset = OS_SECTIONINFO_OFFSET(siFlush->offset);
        DCFlushRange((void*)offset, siFlush->size);
        ICInvalidateRange((void*)offset, siFlush->size);
    }

    return TRUE;
}
BOOL OSLink(OSModuleInfo* newModule, void* bss) {
    u32 i;
    OSSectionInfo* si;
    OSModuleHeader* moduleHeader;
    OSModuleInfo* moduleInfo;
    OSImportInfo* imp;

    moduleHeader = (OSModuleHeader*)newModule;

    if (newModule->version > 2 || (newModule->version >= 2 &&
            ((moduleHeader->align && (u32)newModule % moduleHeader->align != 0) ||
             (moduleHeader->bssAlign && (u32)bss % moduleHeader->bssAlign != 0))))
    {
        return FALSE;
    }

    ENQUEUE_INFO(&__OSModuleInfoList, newModule, link);
    memset(bss, 0, moduleHeader->bssSize);
    newModule->sectionInfoOffset += (u32)moduleHeader;
    moduleHeader->relOffset += (u32)moduleHeader;
    moduleHeader->impOffset += (u32)moduleHeader;

    for (i = 0; i < newModule->numSections; i++) {
        si = &OSGetSectionInfo(newModule)[i];
        if (si->offset != 0) {
            si->offset += (u32)moduleHeader;
        } else if (si->size != 0) {
            si->offset = (u32)bss;
            bss = (void*)((u32)bss + si->size);
        }
    }

    for (imp = (OSImportInfo*)moduleHeader->impOffset;
         imp < (OSImportInfo*)(moduleHeader->impOffset + moduleHeader->impSize); imp++)
    {
        imp->offset += (u32)moduleHeader;
    }

    if (moduleHeader->prologSection != SHN_UNDEF) {
        moduleHeader->prolog +=
            OS_SECTIONINFO_OFFSET(OSGetSectionInfo(newModule)[moduleHeader->prologSection].offset);
    }

    if (moduleHeader->epilogSection != SHN_UNDEF) {
        moduleHeader->epilog +=
            OS_SECTIONINFO_OFFSET(OSGetSectionInfo(newModule)[moduleHeader->epilogSection].offset);
    }

    if (moduleHeader->unresolvedSection != SHN_UNDEF) {
        moduleHeader->unresolved += OS_SECTIONINFO_OFFSET(
            OSGetSectionInfo(newModule)[moduleHeader->unresolvedSection].offset);
    }

    if (__OSStringTable) {
        newModule->nameOffset += (u32)__OSStringTable;
    }

    Relocate(0, moduleHeader);

    for (moduleInfo = __OSModuleInfoList.head; moduleInfo; moduleInfo = moduleInfo->link.next) {
        Relocate(moduleHeader, (OSModuleHeader*)moduleInfo);
        if (moduleInfo != newModule) {
            Relocate((OSModuleHeader*)moduleInfo, moduleHeader);
        }
    }

    OSNotifyLink();
    return TRUE;
}

static BOOL Undo(OSModuleHeader* newModule, OSModuleHeader* module) {
    u32 idNew;
    OSImportInfo* imp;
    OSRel* rel;
    OSSectionInfo* si;
    OSSectionInfo* siFlush;
    u32* p;
    u32 offset;
    u32 x;

    idNew = newModule->info.id;
    for (imp = (OSImportInfo*)module->impOffset;
         imp < (OSImportInfo*)(module->impOffset + module->impSize); imp++)
    {
        if (imp->id == idNew) {
            goto Found;
        }
    }
    return FALSE;

Found:
    siFlush = 0;
    for (rel = (OSRel*)imp->offset; rel->type != R_DOLPHIN_END; rel++) {
        (u8*)p += rel->offset;
        si = &OSGetSectionInfo(newModule)[rel->section];
        offset = OS_SECTIONINFO_OFFSET(si->offset);
        x = 0;
        switch (rel->type) {
        case R_PPC_NONE:
            break;
        case R_PPC_ADDR32:
            *p = x;
            break;
        case R_PPC_ADDR24:
            *p = (*p & ~0x03fffffc) | (x & 0x03fffffc);
            break;
        case R_PPC_ADDR16:
            *(u16*)p = (u16)(x & 0xffff);
            break;
        case R_PPC_ADDR16_LO:
            *(u16*)p = (u16)(x & 0xffff);
            break;
        case R_PPC_ADDR16_HI:
            *(u16*)p = (u16)(((x >> 16) & 0xffff));
            break;
        case R_PPC_ADDR16_HA:
            *(u16*)p = (u16)(((x >> 16) + ((x & 0x8000) ? 1 : 0)) & 0xffff);
            break;
        case R_PPC_REL24:
            if (module->unresolvedSection != SHN_UNDEF) {
                x = (u32)module->unresolved - (u32)p;
            }
            *p = (*p & ~0x03fffffc) | (x & 0x03fffffc);
            break;
        case R_DOLPHIN_NOP:
            break;
        case R_DOLPHIN_SECTION:
            si = &OSGetSectionInfo(module)[rel->section];
            p = (u32*)OS_SECTIONINFO_OFFSET(si->offset);
            if (siFlush) {
                offset = OS_SECTIONINFO_OFFSET(siFlush->offset);
                DCFlushRange((void*)offset, siFlush->size);
                ICInvalidateRange((void*)offset, siFlush->size);
            }
            siFlush = (si->offset & OS_SECTIONINFO_EXEC) ? si : 0;
            break;
        default:
            OSReport("OSUnlink: unknown relocation type %3d\n", rel->type);
            break;
        }
    }

    if (siFlush) {
        offset = OS_SECTIONINFO_OFFSET(siFlush->offset);
        DCFlushRange((void*)offset, siFlush->size);
        ICInvalidateRange((void*)offset, siFlush->size);
    }

    return TRUE;
}

BOOL OSUnlink(OSModuleInfo* oldModule) {
    OSModuleHeader* moduleHeader;
    OSModuleInfo* moduleInfo;
    u32 i;

    moduleHeader = (OSModuleHeader*)oldModule;

    DEQUEUE_INFO(oldModule, &__OSModuleInfoList, link);

    for (moduleInfo = __OSModuleInfoList.head; moduleInfo; moduleInfo = moduleInfo->link.next) {
        Undo(moduleHeader, (OSModuleHeader*)moduleInfo);
    }

    OSNotifyUnlink();
    return TRUE;
}

void __OSModuleInit(void) {
    __OSModuleList.tail = NULL;
    __OSModuleList.head = NULL;
    __OSStringTable = NULL;
}
