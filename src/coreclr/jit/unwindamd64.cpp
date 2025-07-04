// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                              UnwindInfo                                   XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#if defined(TARGET_AMD64)
#ifdef UNIX_AMD64_ABI
short Compiler::mapRegNumToDwarfReg(regNumber reg)
{
    short dwarfReg = DWARF_REG_ILLEGAL;

    switch (reg)
    {
        case REG_RAX:
            dwarfReg = 0;
            break;
        case REG_RCX:
            dwarfReg = 2;
            break;
        case REG_RDX:
            dwarfReg = 1;
            break;
        case REG_RBX:
            dwarfReg = 3;
            break;
        case REG_RSP:
            dwarfReg = 7;
            break;
        case REG_RBP:
            dwarfReg = 6;
            break;
        case REG_RSI:
            dwarfReg = 4;
            break;
        case REG_RDI:
            dwarfReg = 5;
            break;
        case REG_R8:
            dwarfReg = 8;
            break;
        case REG_R9:
            dwarfReg = 9;
            break;
        case REG_R10:
            dwarfReg = 10;
            break;
        case REG_R11:
            dwarfReg = 11;
            break;
        case REG_R12:
            dwarfReg = 12;
            break;
        case REG_R13:
            dwarfReg = 13;
            break;
        case REG_R14:
            dwarfReg = 14;
            break;
        case REG_R15:
            dwarfReg = 15;
            break;
        case REG_R16:
            dwarfReg = 16;
            break;
        case REG_R17:
            dwarfReg = 17;
            break;
        case REG_R18:
            dwarfReg = 18;
            break;
        case REG_R19:
            dwarfReg = 19;
            break;
        case REG_R20:
            dwarfReg = 20;
            break;
        case REG_R21:
            dwarfReg = 21;
            break;
        case REG_R22:
            dwarfReg = 22;
            break;
        case REG_R23:
            dwarfReg = 23;
            break;
        case REG_R24:
            dwarfReg = 24;
            break;
        case REG_R25:
            dwarfReg = 25;
            break;
        case REG_R26:
            dwarfReg = 26;
            break;
        case REG_R27:
            dwarfReg = 27;
            break;
        case REG_R28:
            dwarfReg = 28;
            break;
        case REG_R29:
            dwarfReg = 29;
            break;
        case REG_R30:
            dwarfReg = 30;
            break;
        case REG_R31:
            dwarfReg = 31;
            break;
        default:
            noway_assert(!"unexpected REG_NUM");
    }

    return dwarfReg;
}

#endif // UNIX_AMD64_ABI

//------------------------------------------------------------------------
// Compiler::unwindBegProlog: Initialize the unwind info data structures.
// Called at the beginning of main function or funclet prolog generation.
//
void Compiler::unwindBegProlog()
{
    assert(!compGeneratingUnwindProlog);
    compGeneratingUnwindProlog = true;
#ifdef UNIX_AMD64_ABI
    if (generateCFIUnwindCodes())
    {
        unwindBegPrologCFI();
    }
    else
#endif // UNIX_AMD64_ABI
    {
        unwindBegPrologWindows();
    }
}

void Compiler::unwindBegPrologWindows()
{
    assert(compGeneratingProlog);

    FuncInfoDsc* func = funCurrentFunc();

    // There is only one prolog for a function/funclet, and it comes first. So now is
    // a good time to initialize all the unwind data structures.

    unwindGetFuncLocations(func, true, &func->startLoc, &func->endLoc);

    if (fgFirstColdBlock != nullptr)
    {
        unwindGetFuncLocations(func, false, &func->coldStartLoc, &func->coldEndLoc);
    }

    func->unwindCodeSlot                  = sizeof(func->unwindCodes);
    func->unwindHeader.Version            = 1;
    func->unwindHeader.Flags              = 0;
    func->unwindHeader.CountOfUnwindCodes = 0;
    func->unwindHeader.FrameRegister      = 0;
    func->unwindHeader.FrameOffset        = 0;
}

//------------------------------------------------------------------------
// Compiler::unwindEndProlog: Called at the end of main function or funclet
// prolog generation to indicate there is no more unwind information for this prolog.
//
void Compiler::unwindEndProlog()
{
    assert(compGeneratingProlog);
    assert(compGeneratingUnwindProlog);
    compGeneratingUnwindProlog = false;
}

//------------------------------------------------------------------------
// Compiler::unwindBegEpilog: Called at the beginning of main function or funclet
// epilog generation.
//
void Compiler::unwindBegEpilog()
{
    assert(compGeneratingEpilog);
    assert(!compGeneratingUnwindEpilog);
    compGeneratingUnwindEpilog = true;
}

//------------------------------------------------------------------------
// Compiler::unwindEndEpilog: Called at the end of main function or funclet
// epilog generation.
//
void Compiler::unwindEndEpilog()
{
    assert(compGeneratingEpilog);
    assert(compGeneratingUnwindEpilog);
    compGeneratingUnwindEpilog = false;
}

//------------------------------------------------------------------------
// Compiler::unwindPush: Record a push/save of a register.
//
// Arguments:
//    reg - The register being pushed/saved.
//
void Compiler::unwindPush(regNumber reg)
{
#ifdef UNIX_AMD64_ABI
    if (generateCFIUnwindCodes())
    {
        unwindPushPopCFI(reg);
    }
    else
#endif // UNIX_AMD64_ABI
    {
        unwindPushWindows(reg);
    }
}

//------------------------------------------------------------------------
// Compiler::unwindPush2Windows: Record  push/save of 2 registers simultaneously.
//
// Arguments:
//    reg1 - The first register being pushed/saved.
//    reg2 - The second register being pushed/saved.
//
void Compiler::unwindPush2(regNumber reg1, regNumber reg2)
{
#ifdef UNIX_AMD64_ABI
    if (generateCFIUnwindCodes())
    {
        unwindPush2Pop2CFI(reg1, reg2);
    }
    else
#endif // UNIX_AMD64_ABI
    {
        unwindPush2Windows(reg1, reg2);
    }
}

void Compiler::unwindPushWindows(regNumber reg)
{
    assert(compGeneratingProlog);

    FuncInfoDsc* func = funCurrentFunc();

    assert(func->unwindHeader.Version == 1);            // Can't call this before unwindBegProlog
    assert(func->unwindHeader.CountOfUnwindCodes == 0); // Can't call this after unwindReserve
    assert(func->unwindCodeSlot > sizeof(UNWIND_CODE));
    UNWIND_CODE* code     = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(UNWIND_CODE)];
    unsigned int cbProlog = unwindGetCurrentOffset(func);
    noway_assert((BYTE)cbProlog == cbProlog);
    code->CodeOffset = (BYTE)cbProlog;

    if ((RBM_CALLEE_SAVED & genRegMask(reg))
#if ETW_EBP_FRAMED
        // In case of ETW_EBP_FRAMED defined the REG_FPBASE (RBP)
        // is excluded from the callee-save register list.
        // Make sure the register gets PUSH unwind info in this case,
        // since it is pushed as a frame register.
        || (reg == REG_FPBASE)
#endif // ETW_EBP_FRAMED
    )
    {
        code->UnwindOp = UWOP_PUSH_NONVOL;
        code->OpInfo   = (BYTE)reg;
    }
    else
    {
        // Push of a volatile register is just a small stack allocation
        code->UnwindOp = UWOP_ALLOC_SMALL;
        code->OpInfo   = 0;
    }
}

//------------------------------------------------------------------------
// Compiler::unwindPush2Windows: Record  push/save of 2 registers simultaneously in windows OS.
//
// Arguments:
//    reg1 - The first register being pushed/saved.
//    reg2 - The second register being pushed/saved.
//
void Compiler::unwindPush2Windows(regNumber reg1, regNumber reg2)
{
    // ToDo: This is a placeholder till Windows OS has unwind support for push2/pop2.
    unwindPushWindows(reg1);
    unwindPushWindows(reg2);
}

#ifdef UNIX_AMD64_ABI
#endif // UNIX_AMD64_ABI

//------------------------------------------------------------------------
// Compiler::unwindAllocStack: Record a stack frame allocation (sub sp, X).
//
// Arguments:
//    size - The size of the stack frame allocation (the amount subtracted from the stack pointer).
//
void Compiler::unwindAllocStack(unsigned size)
{
#ifdef UNIX_AMD64_ABI
    if (generateCFIUnwindCodes())
    {
        unwindAllocStackCFI(size);
    }
    else
#endif // UNIX_AMD64_ABI
    {
        unwindAllocStackWindows(size);
    }
}

void Compiler::unwindAllocStackWindows(unsigned size)
{
    assert(compGeneratingProlog);

    FuncInfoDsc* func = funCurrentFunc();

    assert(func->unwindHeader.Version == 1);            // Can't call this before unwindBegProlog
    assert(func->unwindHeader.CountOfUnwindCodes == 0); // Can't call this after unwindReserve
    assert(size % 8 == 0);                              // Stack size is *always* 8 byte aligned
    UNWIND_CODE* code;
    if (size <= 128)
    {
        assert(func->unwindCodeSlot > sizeof(UNWIND_CODE));
        code           = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(UNWIND_CODE)];
        code->UnwindOp = UWOP_ALLOC_SMALL;
        code->OpInfo   = (size - 8) / 8;
    }
    else if (size <= 0x7FFF8)
    {
        assert(func->unwindCodeSlot > (sizeof(UNWIND_CODE) + sizeof(USHORT)));
        USHORT* codedSize = (USHORT*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(USHORT)];
        *codedSize        = (USHORT)(size / 8);
        code              = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(UNWIND_CODE)];
        code->UnwindOp    = UWOP_ALLOC_LARGE;
        code->OpInfo      = 0;
    }
    else
    {
        assert(func->unwindCodeSlot > (sizeof(UNWIND_CODE) + sizeof(ULONG)));
        ULONG* codedSize = (ULONG*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(ULONG)];
        *codedSize       = size;
        code             = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(UNWIND_CODE)];
        code->UnwindOp   = UWOP_ALLOC_LARGE;
        code->OpInfo     = 1;
    }
    unsigned int cbProlog = unwindGetCurrentOffset(func);
    noway_assert((BYTE)cbProlog == cbProlog);
    code->CodeOffset = (BYTE)cbProlog;
}

//------------------------------------------------------------------------
// Compiler::unwindSetFrameReg: Record a frame register.
//
// Arguments:
//    reg    - The register being set as the frame register.
//    offset - The offset from the current stack pointer that the frame pointer will point at.
//
void Compiler::unwindSetFrameReg(regNumber reg, unsigned offset)
{
#ifdef UNIX_AMD64_ABI
    if (generateCFIUnwindCodes())
    {
        unwindSetFrameRegCFI(reg, offset);
    }
    else
#endif // UNIX_AMD64_ABI
    {
        unwindSetFrameRegWindows(reg, offset);
    }
}

void Compiler::unwindSetFrameRegWindows(regNumber reg, unsigned offset)
{
    assert(compGeneratingProlog);

    FuncInfoDsc* func = funCurrentFunc();

    assert(func->unwindHeader.Version == 1);            // Can't call this before unwindBegProlog
    assert(func->unwindHeader.CountOfUnwindCodes == 0); // Can't call this after unwindReserve
    unsigned int cbProlog = unwindGetCurrentOffset(func);
    noway_assert((BYTE)cbProlog == cbProlog);

    func->unwindHeader.FrameRegister = (BYTE)reg;

#ifdef UNIX_AMD64_ABI
    if (offset > 240)
    {
        // On Unix only, we have a CLR-only extension to the AMD64 unwind codes: UWOP_SET_FPREG_LARGE.
        // It has a 32-bit offset (scaled). You must set UNWIND_INFO.FrameOffset to 15. The 32-bit
        // offset follows in 2 UNWIND_CODE fields.

        assert(func->unwindCodeSlot > (sizeof(UNWIND_CODE) + sizeof(ULONG)));
        ULONG* codedSize = (ULONG*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(ULONG)];
        assert(offset % 16 == 0);
        *codedSize = offset / 16;

        UNWIND_CODE* code              = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(UNWIND_CODE)];
        code->CodeOffset               = (BYTE)cbProlog;
        code->OpInfo                   = 0;
        code->UnwindOp                 = UWOP_SET_FPREG_LARGE;
        func->unwindHeader.FrameOffset = 15;
    }
    else
#endif // UNIX_AMD64_ABI
    {
        assert(func->unwindCodeSlot > sizeof(UNWIND_CODE));
        UNWIND_CODE* code = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(UNWIND_CODE)];
        code->CodeOffset  = (BYTE)cbProlog;
        code->OpInfo      = 0;
        code->UnwindOp    = UWOP_SET_FPREG;
        assert(offset <= 240);
        assert(offset % 16 == 0);
        func->unwindHeader.FrameOffset = offset / 16;
    }
}

//------------------------------------------------------------------------
// Compiler::unwindSaveReg: Record a register save.
//
// Arguments:
//    reg    - The register being saved.
//    offset - The offset from the current stack pointer where the register is being saved.
//
void Compiler::unwindSaveReg(regNumber reg, unsigned offset)
{
#ifdef UNIX_AMD64_ABI
    if (generateCFIUnwindCodes())
    {
        unwindSaveRegCFI(reg, offset);
    }
    else
#endif // UNIX_AMD64_ABI
    {
        unwindSaveRegWindows(reg, offset);
    }
}

void Compiler::unwindSaveRegWindows(regNumber reg, unsigned offset)
{
    assert(compGeneratingProlog);

    FuncInfoDsc* func = funCurrentFunc();

    assert(func->unwindHeader.Version == 1);            // Can't call this before unwindBegProlog
    assert(func->unwindHeader.CountOfUnwindCodes == 0); // Can't call this after unwindReserve
    if (RBM_CALLEE_SAVED & genRegMask(reg))
    {
        UNWIND_CODE* code;
        if (offset < 0x80000)
        {
            assert(func->unwindCodeSlot > (sizeof(UNWIND_CODE) + sizeof(USHORT)));
            USHORT* codedSize = (USHORT*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(USHORT)];
            code              = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(UNWIND_CODE)];

            // As per AMD64 ABI, if saving entire xmm reg, then offset need to be scaled by 16.
            if (genIsValidFloatReg(reg))
            {
                *codedSize     = (USHORT)(offset / 16);
                code->UnwindOp = UWOP_SAVE_XMM128;
            }
            else
            {
                *codedSize     = (USHORT)(offset / 8);
                code->UnwindOp = UWOP_SAVE_NONVOL;
            }
        }
        else
        {
            assert(func->unwindCodeSlot > (sizeof(UNWIND_CODE) + sizeof(ULONG)));
            ULONG* codedSize = (ULONG*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(ULONG)];
            *codedSize       = offset;
            code             = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot -= sizeof(UNWIND_CODE)];
            code->UnwindOp   = (genIsValidFloatReg(reg)) ? UWOP_SAVE_XMM128_FAR : UWOP_SAVE_NONVOL_FAR;
        }
        unsigned unwindRegNum;
        if (genIsValidFloatReg(reg))
        {
            unwindRegNum = reg - XMMBASE;
        }
        else
        {
            assert(genIsValidIntReg(reg));
            unwindRegNum = reg;
        }
        // We only add unwind codes for non-volatile registers and for x86/x64,
        // the max registers index for a non-volatile register is 15.
        assert(unwindRegNum <= 15);
        code->OpInfo = (UCHAR)unwindRegNum;
        assert((unsigned)code->OpInfo == unwindRegNum);

        unsigned int cbProlog = unwindGetCurrentOffset(func);
        noway_assert((BYTE)cbProlog == cbProlog);
        code->CodeOffset = (BYTE)cbProlog;
    }
}

#ifdef UNIX_AMD64_ABI
void Compiler::unwindSaveRegCFI(regNumber reg, unsigned offset)
{
    assert(compGeneratingProlog);

    if (RBM_CALLEE_SAVED & genRegMask(reg))
    {
        FuncInfoDsc* func = funCurrentFunc();

        unsigned int cbProlog = unwindGetCurrentOffset(func);
        createCfiCode(func, cbProlog, CFI_REL_OFFSET, mapRegNumToDwarfReg(reg), offset);
    }
}
#endif // UNIX_AMD64_ABI

#ifdef DEBUG

//------------------------------------------------------------------------
// DumpUnwindInfo: Dump the unwind data.
//
// Arguments:
//    isHotCode   - true if this unwind data is for the hot section, false otherwise.
//    startOffset - byte offset of the code start that this unwind data represents.
//    endOffset   - byte offset of the code end   that this unwind data represents.
//    pHeader     - pointer to the unwind data blob.
//
void DumpUnwindInfo(bool                     isHotCode,
                    UNATIVE_OFFSET           startOffset,
                    UNATIVE_OFFSET           endOffset,
                    const UNWIND_INFO* const pHeader)
{
    printf("Unwind Info%s:\n", isHotCode ? "" : " COLD");
    printf("  >> Start offset   : 0x%06x (not in unwind data)\n", dspOffset(startOffset));
    printf("  >>   End offset   : 0x%06x (not in unwind data)\n", dspOffset(endOffset));

    if (pHeader == nullptr)
    {
        // Cold AMD64 code doesn't have unwind info; the VM creates chained unwind info.
        assert(!isHotCode);
        return;
    }

    printf("  Version           : %u\n", pHeader->Version);
    printf("  Flags             : 0x%02x", pHeader->Flags);
    if (pHeader->Flags)
    {
        const UCHAR flags = pHeader->Flags;
        printf(" (");
        if (flags & UNW_FLAG_EHANDLER)
        {
            printf(" UNW_FLAG_EHANDLER");
        }
        if (flags & UNW_FLAG_UHANDLER)
        {
            printf(" UNW_FLAG_UHANDLER");
        }
        if (flags & UNW_FLAG_CHAININFO)
        {
            printf(" UNW_FLAG_CHAININFO");
        }
        printf(")");
    }
    printf("\n");
    printf("  SizeOfProlog      : 0x%02X\n", pHeader->SizeOfProlog);
    printf("  CountOfUnwindCodes: %u\n", pHeader->CountOfUnwindCodes);
    printf("  FrameRegister     : %s (%u)\n",
           (pHeader->FrameRegister == 0) ? "none" : getRegName(pHeader->FrameRegister),
           pHeader->FrameRegister); // RAX (0) is not allowed as a frame register
    if (pHeader->FrameRegister == 0)
    {
        printf("  FrameOffset       : N/A (no FrameRegister) (Value=%u)\n", pHeader->FrameOffset);
    }
    else
    {
        printf("  FrameOffset       : %u * 16 = 0x%02X\n", pHeader->FrameOffset, pHeader->FrameOffset * 16);
    }
    printf("  UnwindCodes       :\n");

    for (unsigned i = 0; i < pHeader->CountOfUnwindCodes; i++)
    {
        const UNWIND_CODE* const pCode = &(pHeader->UnwindCode[i]);
        switch (pCode->UnwindOp)
        {
            case UWOP_PUSH_NONVOL:
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_PUSH_NONVOL (%u)     OpInfo: %s (%u)\n",
                       pCode->CodeOffset, pCode->UnwindOp, getRegName(pCode->OpInfo), pCode->OpInfo);
                break;

            case UWOP_ALLOC_LARGE:
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_ALLOC_LARGE (%u)     OpInfo: %u - ", pCode->CodeOffset,
                       pCode->UnwindOp, pCode->OpInfo);
                if (pCode->OpInfo == 0)
                {
                    i++;
                    printf("Scaled small  \n      Size: %u * 8 = %u = 0x%05X\n", pHeader->UnwindCode[i].FrameOffset,
                           pHeader->UnwindCode[i].FrameOffset * 8, pHeader->UnwindCode[i].FrameOffset * 8);
                }
                else if (pCode->OpInfo == 1)
                {
                    i++;
                    printf("Unscaled large\n      Size: %u = 0x%08X\n\n", *(ULONG*)&(pHeader->UnwindCode[i]),
                           *(ULONG*)&(pHeader->UnwindCode[i]));
                    i++;
                }
                else
                {
                    printf("Unknown\n");
                }
                break;

            case UWOP_ALLOC_SMALL:
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_ALLOC_SMALL (%u)     OpInfo: %u * 8 + 8 = %u = 0x%02X\n",
                       pCode->CodeOffset, pCode->UnwindOp, pCode->OpInfo, pCode->OpInfo * 8 + 8, pCode->OpInfo * 8 + 8);
                break;

            case UWOP_SET_FPREG:
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_SET_FPREG (%u)       OpInfo: Unused (%u)\n",
                       pCode->CodeOffset, pCode->UnwindOp, pCode->OpInfo); // This should be zero
                break;

#ifdef UNIX_AMD64_ABI

            case UWOP_SET_FPREG_LARGE:
            {
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_SET_FPREG_LARGE (%u) OpInfo: Unused (%u)\n",
                       pCode->CodeOffset, pCode->UnwindOp, pCode->OpInfo); // This should be zero
                i++;
                unsigned offset = *(ULONG*)&(pHeader->UnwindCode[i]);
                i++;
                printf("      Scaled Offset: %u * 16 = %u = 0x%08X\n", offset, offset * 16, offset * 16);
                if ((offset & 0xF0000000) != 0)
                {
                    printf("      Illegal unscaled offset: too large\n");
                }
            }
            break;

#endif // UNIX_AMD64_ABI

            case UWOP_SAVE_NONVOL:
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_SAVE_NONVOL (%u)     OpInfo: %s (%u)\n",
                       pCode->CodeOffset, pCode->UnwindOp, getRegName(pCode->OpInfo), pCode->OpInfo);
                i++;
                printf("      Scaled Small Offset: %u * 8 = %u = 0x%05X\n", pHeader->UnwindCode[i].FrameOffset,
                       pHeader->UnwindCode[i].FrameOffset * 8, pHeader->UnwindCode[i].FrameOffset * 8);
                break;

            case UWOP_SAVE_NONVOL_FAR:
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_SAVE_NONVOL_FAR (%u) OpInfo: %s (%u)\n",
                       pCode->CodeOffset, pCode->UnwindOp, getRegName(pCode->OpInfo), pCode->OpInfo);
                i++;
                printf("      Unscaled Large Offset: 0x%08X\n\n", *(ULONG*)&(pHeader->UnwindCode[i]));
                i++;
                break;

            case UWOP_SAVE_XMM128:
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_SAVE_XMM128 (%u)     OpInfo: XMM%u (%u)\n",
                       pCode->CodeOffset, pCode->UnwindOp, pCode->OpInfo, pCode->OpInfo);
                i++;
                printf("      Scaled Small Offset: %u * 16 = %u = 0x%05X\n", pHeader->UnwindCode[i].FrameOffset,
                       pHeader->UnwindCode[i].FrameOffset * 16, pHeader->UnwindCode[i].FrameOffset * 16);
                break;

            case UWOP_SAVE_XMM128_FAR:
                printf("    CodeOffset: 0x%02X UnwindOp: UWOP_SAVE_XMM128_FAR (%u) OpInfo: XMM%u (%u)\n",
                       pCode->CodeOffset, pCode->UnwindOp, pCode->OpInfo, pCode->OpInfo);
                i++;
                printf("      Unscaled Large Offset: 0x%08X\n\n", *(ULONG*)&(pHeader->UnwindCode[i]));
                i++;
                break;

            case UWOP_EPILOG:
            case UWOP_SPARE_CODE:
            case UWOP_PUSH_MACHFRAME:
            default:
                printf("    Unrecognized UNWIND_CODE: 0x%04X\n", *(USHORT*)pCode);
                break;
        }
    }
}

#endif // DEBUG

//------------------------------------------------------------------------
// Compiler::unwindReserve: Ask the VM to reserve space for the unwind information
// for the function and all its funclets. Called once, just before asking the VM
// for memory and emitting the generated code. Calls unwindReserveFunc() to handle
// the main function and each of the funclets, in turn.
//
void Compiler::unwindReserve()
{
    assert(!compGeneratingProlog);
    assert(!compGeneratingEpilog);

    assert(compFuncInfoCount > 0);
    for (unsigned funcIdx = 0; funcIdx < compFuncInfoCount; funcIdx++)
    {
        unwindReserveFunc(funGetFunc(funcIdx));
    }
}

//------------------------------------------------------------------------
// Compiler::unwindReserveFunc: Reserve the unwind information from the VM for a
// given main function or funclet.
//
// Arguments:
//    func - The main function or funclet to reserve unwind info for.
//
void Compiler::unwindReserveFunc(FuncInfoDsc* func)
{
#ifdef DEBUG
    // If fake-splitting, treat all unwind info as hot.
    if (JitConfig.JitFakeProcedureSplitting())
    {
        unwindReserveFuncHelper(func, true);
        return;
    }
#endif // DEBUG

    if (func->funKind == FUNC_ROOT)
    {
        unwindReserveFuncHelper(func, true);

        // If the function's main body is split, reserve unwind info of size 0 for the cold section.
        // If only funclets are cold, the main body is hot, so don't make a second call.
        const bool isFunctionSplit = ((fgFirstColdBlock != nullptr) && (fgFirstColdBlock != fgFirstFuncletBB));
        if (isFunctionSplit)
        {
            unwindReserveFuncHelper(func, false);
        }
    }
    else
    {
        // Make only one call for funclets.
        // If function is split and has EH, the funclets will be cold.
        const bool isFuncletHot = (fgFirstColdBlock == nullptr);
        unwindReserveFuncHelper(func, isFuncletHot);
    }
}

//------------------------------------------------------------------------
// Compiler::unwindReserveFuncHelper: Reserve the unwind information from the VM for a
// given main function or funclet, for either the hot or the cold section.
//
// Arguments:
//    func      - The main function or funclet to reserve unwind info for.
//    isHotCode - 'true' to reserve the hot section, 'false' to reserve the cold section.
//
void Compiler::unwindReserveFuncHelper(FuncInfoDsc* func, bool isHotCode)
{
    const bool isFunclet       = (func->funKind != FUNC_ROOT);
    DWORD      unwindCodeBytes = 0;

    if (isHotCode || isFunclet)
    {
#ifdef UNIX_AMD64_ABI
        if (generateCFIUnwindCodes())
        {
            unwindCodeBytes = (DWORD)(func->cfiCodes->size() * sizeof(CFI_CODE));
        }
        else
#endif // UNIX_AMD64_ABI
        {
            assert(func->unwindHeader.Version == 1);            // Can't call this before unwindBegProlog
            assert(func->unwindHeader.CountOfUnwindCodes == 0); // Only call this once per prolog

            // Set the size of the prolog to be the last encoded action
            if (func->unwindCodeSlot < sizeof(func->unwindCodes))
            {
                UNWIND_CODE* code               = (UNWIND_CODE*)&func->unwindCodes[func->unwindCodeSlot];
                func->unwindHeader.SizeOfProlog = code->CodeOffset;
            }
            else
            {
                func->unwindHeader.SizeOfProlog = 0;
            }
            func->unwindHeader.CountOfUnwindCodes =
                (BYTE)((sizeof(func->unwindCodes) - func->unwindCodeSlot) / sizeof(UNWIND_CODE));

            // Prepend the unwindHeader onto the unwind codes
            assert(func->unwindCodeSlot >= offsetof(UNWIND_INFO, UnwindCode));

            func->unwindCodeSlot -= offsetof(UNWIND_INFO, UnwindCode);
            UNWIND_INFO* pHeader = (UNWIND_INFO*)&func->unwindCodes[func->unwindCodeSlot];
            memcpy(pHeader, &func->unwindHeader, offsetof(UNWIND_INFO, UnwindCode));

            unwindCodeBytes = sizeof(func->unwindCodes) - func->unwindCodeSlot;
        }
    }

    const bool isColdCode = !isHotCode;
    eeReserveUnwindInfo(isFunclet, isColdCode, unwindCodeBytes);
}

//------------------------------------------------------------------------
// Compiler::unwindEmit: Report all the unwind information to the VM.
//
// Arguments:
//    pHotCode  - Pointer to the beginning of the memory with the function and funclet hot  code.
//    pColdCode - Pointer to the beginning of the memory with the function and funclet cold code.
//
void Compiler::unwindEmit(void* pHotCode, void* pColdCode)
{
    assert(!compGeneratingProlog);
    assert(!compGeneratingEpilog);

    assert(compFuncInfoCount > 0);
    for (unsigned funcIdx = 0; funcIdx < compFuncInfoCount; funcIdx++)
    {
        unwindEmitFunc(funGetFunc(funcIdx), pHotCode, pColdCode);
    }
}

//------------------------------------------------------------------------
// Compiler::unwindEmitFuncHelper: Report the unwind information to the VM for a
// given main function or funclet, for either the hot or cold section.
//
// Arguments:
//    func      - The main function or funclet to reserve unwind info for.
//    pHotCode  - Pointer to the beginning of the memory with the function and funclet hot  code.
//    pColdCode - Pointer to the beginning of the memory with the function and funclet cold code.
//                Ignored if 'isHotCode' is true.
//    isHotCode - 'true' to report the hot section, 'false' to report the cold section.
//
void Compiler::unwindEmitFuncHelper(FuncInfoDsc* func, void* pHotCode, void* pColdCode, bool isHotCode)
{
    UNATIVE_OFFSET startOffset;
    UNATIVE_OFFSET endOffset;
    DWORD          unwindCodeBytes = 0;
    BYTE*          pUnwindBlock    = nullptr;

    if (isHotCode)
    {
        if (func->startLoc == nullptr)
        {
            startOffset = 0;
        }
        else
        {
            startOffset = func->startLoc->CodeOffset(GetEmitter());
        }

        if (func->endLoc == nullptr)
        {
            endOffset = info.compNativeCodeSize;
        }
        else
        {
            endOffset = func->endLoc->CodeOffset(GetEmitter());
        }
    }
    else
    {
        assert(fgFirstColdBlock != nullptr);

        if (func->coldStartLoc == nullptr)
        {
            startOffset = 0;
        }
        else
        {
            startOffset = func->coldStartLoc->CodeOffset(GetEmitter());
        }

        if (func->coldEndLoc == nullptr)
        {
            endOffset = info.compNativeCodeSize;
        }
        else
        {
            endOffset = func->coldEndLoc->CodeOffset(GetEmitter());
        }
    }

    if (isHotCode || (func->funKind != FUNC_ROOT))
    {
#ifdef UNIX_AMD64_ABI
        if (generateCFIUnwindCodes())
        {
            DWORD size = (DWORD)func->cfiCodes->size();
            if (size > 0)
            {
                unwindCodeBytes = size * sizeof(CFI_CODE);
                pUnwindBlock    = (BYTE*)&(*func->cfiCodes)[0];
            }
        }
        else
#endif // UNIX_AMD64_ABI
        {
            unwindCodeBytes = sizeof(func->unwindCodes) - func->unwindCodeSlot;

#ifdef DEBUG
            UNWIND_INFO* pUnwindInfo = (UNWIND_INFO*)(&func->unwindCodes[func->unwindCodeSlot]);
            DWORD        unwindCodeBytesSpecified =
                offsetof(UNWIND_INFO, UnwindCode) +
                pUnwindInfo->CountOfUnwindCodes * sizeof(UNWIND_CODE); // This is what the unwind codes themselves say;
                                                                       // it better match what we tell the VM.
            assert(unwindCodeBytes == unwindCodeBytesSpecified);
#endif // DEBUG

            pUnwindBlock = &func->unwindCodes[func->unwindCodeSlot];
        }
    }

#ifdef DEBUG
    if (opts.dspUnwind)
    {
#ifdef UNIX_AMD64_ABI
        if (generateCFIUnwindCodes())
        {
            DumpCfiInfo(isHotCode, startOffset, endOffset, unwindCodeBytes, (const CFI_CODE* const)pUnwindBlock);
        }
        else
#endif // UNIX_AMD64_ABI
        {
            DumpUnwindInfo(isHotCode, startOffset, endOffset, (const UNWIND_INFO* const)pUnwindBlock);
        }
    }
#endif // DEBUG

    // Adjust for cold or hot code:
    // 1. The VM doesn't want the cold code pointer unless this is cold code.
    // 2. The startOffset and endOffset need to be from the base of the hot section for hot code
    //    and from the base of the cold section for cold code

    if (isHotCode)
    {
#ifdef DEBUG
        if (JitConfig.JitFakeProcedureSplitting() && (fgFirstColdBlock != nullptr))
        {
            assert(endOffset <= info.compNativeCodeSize);
        }
        else
#endif // DEBUG
        {
            assert(endOffset <= info.compTotalHotCodeSize);
        }

        pColdCode = nullptr;
    }
    else
    {
        assert(startOffset >= info.compTotalHotCodeSize);
        startOffset -= info.compTotalHotCodeSize;
        endOffset -= info.compTotalHotCodeSize;
    }

    eeAllocUnwindInfo((BYTE*)pHotCode, (BYTE*)pColdCode, startOffset, endOffset, unwindCodeBytes, pUnwindBlock,
                      (CorJitFuncKind)func->funKind);
}

//------------------------------------------------------------------------
// Compiler::unwindEmitFunc: Report the unwind information to the VM for a
// given main function or funclet. Reports the hot section, then the cold
// section if necessary.
//
// Arguments:
//    func      - The main function or funclet to reserve unwind info for.
//    pHotCode  - Pointer to the beginning of the memory with the function and funclet hot  code.
//    pColdCode - Pointer to the beginning of the memory with the function and funclet cold code.
//
void Compiler::unwindEmitFunc(FuncInfoDsc* func, void* pHotCode, void* pColdCode)
{
    // Verify that the JIT enum is in sync with the JIT-EE interface enum
    static_assert_no_msg(FUNC_ROOT == (FuncKind)CORJIT_FUNC_ROOT);
    static_assert_no_msg(FUNC_HANDLER == (FuncKind)CORJIT_FUNC_HANDLER);
    static_assert_no_msg(FUNC_FILTER == (FuncKind)CORJIT_FUNC_FILTER);

#ifdef DEBUG
    // If fake-splitting, treat all unwind info as hot.
    if (JitConfig.JitFakeProcedureSplitting())
    {
        unwindEmitFuncHelper(func, pHotCode, pColdCode, true);
        return;
    }
#endif // DEBUG

    if (func->funKind == FUNC_ROOT)
    {
        unwindEmitFuncHelper(func, pHotCode, pColdCode, true);

        // If the function's main body is split, reserve unwind info of size 0 for the cold section.
        // If only funclets are cold, the main body is hot, so don't make a second call.
        const bool isFunctionSplit = ((fgFirstColdBlock != nullptr) && (fgFirstColdBlock != fgFirstFuncletBB));
        if (isFunctionSplit)
        {
            unwindEmitFuncHelper(func, pHotCode, pColdCode, false);
        }
    }
    else
    {
        // Make only one call for funclets.
        // If function is split and has EH, the funclets will be cold.
        const bool isFuncletHot = (fgFirstColdBlock == nullptr);
        unwindEmitFuncHelper(func, pHotCode, pColdCode, isFuncletHot);
    }
}

#endif // TARGET_AMD64
