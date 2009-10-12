//
// x86 architecture description
//
//  Copyright (C) 2002-2007  Peter Johnson
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
#include "X86Arch.h"

#include "util.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include "yasmx/Support/Compose.h"
#include "yasmx/Support/errwarn.h"
#include "yasmx/Support/nocase.h"
#include "yasmx/Support/registry.h"
#include "yasmx/Bytes.h"
#include "yasmx/Bytes_util.h"
#include "yasmx/Directive.h"
#include "yasmx/Expr.h"
#include "yasmx/IntNum.h"
#include "yasmx/Object.h"
#include "yasmx/NameValue.h"

#include "X86EffAddr.h"
#include "X86RegisterGroup.h"


namespace yasm
{
namespace arch
{
namespace x86
{

#ifndef NELEMS
#define NELEMS(x) (sizeof(x)/sizeof(x[0]))
#endif
static const unsigned int reg_count[X86Register::TYPE_COUNT] =
    {16,    // REG8
     8,     // REG8X
     16,    // REG16
     16,    // REG32
     16,    // REG64
     8,     // FPUREG
     8,     // MMXREG
     16,    // XMMREG
     16,    // YMMREG
     16,    // CRREG
     8,     // DRREG
     8,     // TRREG
     1};    // RIP

X86Arch::X86Arch(const ArchModule& module)
    : Arch(module),
      m_amd64_machine(false),
      m_parser(PARSER_UNKNOWN),
      m_mode_bits(0),
      m_force_strict(false),
      m_default_rel(false),
      m_nop(NOP_BASIC)
{
    // default to all instructions/features enabled
    m_active_cpu.set();

    // create registers
    for (unsigned int i=0; i<NELEMS(m_reg); i++)
    {
        m_reg[i] = new X86Register*[reg_count[i]];
        for (unsigned int j=0; j<reg_count[i]; j++)
            m_reg[i][j] = new X86Register(static_cast<X86Register::Type>(i), j);
    }

    // create register groups
    for (unsigned int i=0; i<NELEMS(m_reg_group); i++)
        m_reg_group[i] = 0;
    m_reg_group[X86Register::FPUREG] =
        new X86RegisterGroup(BIND::bind(&X86Arch::getModeBits, this),
                             m_reg[X86Register::FPUREG],
                             reg_count[X86Register::FPUREG]);
    m_reg_group[X86Register::MMXREG] =
        new X86RegisterGroup(BIND::bind(&X86Arch::getModeBits, this),
                             m_reg[X86Register::MMXREG],
                             reg_count[X86Register::MMXREG]);
    m_reg_group[X86Register::XMMREG] =
        new X86RegisterGroup(BIND::bind(&X86Arch::getModeBits, this),
                             m_reg[X86Register::XMMREG],
                             reg_count[X86Register::XMMREG]);
    m_reg_group[X86Register::YMMREG] =
        new X86RegisterGroup(BIND::bind(&X86Arch::getModeBits, this),
                             m_reg[X86Register::YMMREG],
                             reg_count[X86Register::YMMREG]);

    // create segment registers
    static const unsigned char segreg_prefix[NELEMS(m_segreg)] =
        {0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65};
    for (unsigned int i=0; i<NELEMS(m_segreg); i++)
    {
        m_segreg[i] =
            new X86SegmentRegister(static_cast<X86SegmentRegister::Type>(i),
                                   segreg_prefix[i]);
    }

    // create target modifiers
    for (unsigned int i=0; i<NELEMS(m_targetmod); i++)
    {
        m_targetmod[i] =
            new X86TargetModifier(static_cast<X86TargetModifier::Type>(i));
    }
}

bool
X86Arch::setParser(const llvm::StringRef& parser)
{
    if (String::NocaseEqual(parser, "nasm"))
        m_parser = PARSER_NASM;
    else if (String::NocaseEqual(parser, "gas") ||
             String::NocaseEqual(parser, "gnu"))
        m_parser = PARSER_GAS;
    else
        return false;
    return true;
}

bool
X86Arch::setMachine(const llvm::StringRef& machine)
{
    if (String::NocaseEqual(machine, "x86"))
        m_amd64_machine = false;
    else if (String::NocaseEqual(machine, "amd64"))
        m_amd64_machine = true;
    else
        return false;
    return true;
}

X86Arch::~X86Arch()
{
    for (unsigned int i=0; i<NELEMS(m_targetmod); i++)
        delete m_targetmod[i];
    for (unsigned int i=0; i<NELEMS(m_segreg); i++)
        delete m_segreg[i];
    for (unsigned int i=0; i<NELEMS(m_reg_group); i++)
        delete m_reg_group[i];
    for (unsigned int i=0; i<NELEMS(m_reg); i++)
    {
        for (unsigned int j=0; j<reg_count[i]; j++)
            delete m_reg[i][j];
        delete [] m_reg[i];
    }
}

std::string
X86Arch::getMachine() const
{
    if (m_amd64_machine)
        return "amd64";
    else
        return "x86";
}

ArchModule::MachineNames
X86Arch::getMachines()
{
    ArchModule::MachineNames machines;
    machines.push_back(std::make_pair("x86", "IA-32 and derivatives"));
    machines.push_back(std::make_pair("amd64", "AMD64"));
    return machines;
}

unsigned int
X86Arch::getAddressSize() const
{
    if (m_mode_bits != 0)
        return m_mode_bits;
    if (m_amd64_machine)
        return 64;
    else
        return 32;
}

bool
X86Arch::setVar(const llvm::StringRef& var, unsigned long val)
{
    if (String::NocaseEqual(var, "mode_bits"))
        m_mode_bits = static_cast<unsigned int>(val);
    else if (String::NocaseEqual(var, "force_strict"))
        m_force_strict = (val != 0);
    else if (String::NocaseEqual(var, "default_rel"))
    {
        if (m_mode_bits != 64)
            setWarn(WARN_GENERAL,
                    N_("ignoring default rel in non-64-bit mode"));
        else
            m_default_rel = (val != 0);
    }
    else
        return true;
    return false;
}

void
X86Arch::DirCpu(DirectiveInfo& info)
{
    for (NameValues::const_iterator nv=info.getNameValues().begin(),
         end=info.getNameValues().end(); nv != end; ++nv)
    {
        if (nv->isString())
            ParseCpu(nv->getString());
        else if (nv->isExpr())
        {
            Expr e = nv->getExpr(info.getObject(), info.getSource());
            if (!e.isIntNum())
                throw SyntaxError(String::Compose(
                    N_("invalid argument to [%1]"), "CPU"));
            else
            {
                llvm::SmallString<128> ss;
                llvm::raw_svector_ostream oss(ss);
                oss << e.getIntNum().getUInt();
                ParseCpu(oss.str());
            }
        }
        else
            throw SyntaxError(String::Compose(N_("invalid argument to [%1]"),
                                              "CPU"));
    }
}

void
X86Arch::DirBits(DirectiveInfo& info)
{
    NameValues::const_iterator nv = info.getNameValues().begin();
    if (nv != info.getNameValues().end() && nv->isExpr())
    {
        Expr e = nv->getExpr(info.getObject(), info.getSource());
        if (e.isIntNum())
        {
            unsigned long v = e.getIntNum().getUInt();
            if (v == 16 || v == 32 || v == 64)
            {
                m_mode_bits = v;
                return;
            }
        }
    }

    throw ValueError(String::Compose(N_("invalid argument to [%1]"), "BITS"));
}

void
X86Arch::DirCode16(DirectiveInfo& info)
{
    m_mode_bits = 16;
}

void
X86Arch::DirCode32(DirectiveInfo& info)
{
    m_mode_bits = 32;
}

void
X86Arch::DirCode64(DirectiveInfo& info)
{
    m_mode_bits = 64;
}

const unsigned char **
X86Arch::getFill() const
{
    // Fill patterns that GAS uses.
    static const unsigned char fill16_1[1] =
        {0x90};                                 // 1 - nop
    static const unsigned char fill16_2[2] =
        {0x89, 0xf6};                           // 2 - mov si, si
    static const unsigned char fill16_3[3] =
        {0x8d, 0x74, 0x00};                     // 3 - lea si, [si+byte 0]
    static const unsigned char fill16_4[4] =
        {0x8d, 0xb4, 0x00, 0x00};               // 4 - lea si, [si+word 0]
    static const unsigned char fill16_5[5] =
        {0x90,                                  // 5 - nop
         0x8d, 0xb4, 0x00, 0x00};               //     lea si, [si+word 0]
    static const unsigned char fill16_6[6] =
        {0x89, 0xf6,                            // 6 - mov si, si
         0x8d, 0xbd, 0x00, 0x00};               //     lea di, [di+word 0]
    static const unsigned char fill16_7[7] =
        {0x8d, 0x74, 0x00,                      // 7 - lea si, [si+byte 0]
         0x8d, 0xbd, 0x00, 0x00};               //     lea di, [di+word 0]
    static const unsigned char fill16_8[8] =
        {0x8d, 0xb4, 0x00, 0x00,                // 8 - lea si, [si+word 0]
         0x8d, 0xbd, 0x00, 0x00};               //     lea di, [di+word 0]
    static const unsigned char fill16_9[9] =
        {0xeb, 0x07, 0x90, 0x90, 0x90, 0x90,    // 9 - jmp $+9; nop fill
         0x90, 0x90, 0x90};
    static const unsigned char fill16_10[10] =
        {0xeb, 0x08, 0x90, 0x90, 0x90, 0x90,    // 10 - jmp $+10; nop fill
         0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill16_11[11] =
        {0xeb, 0x09, 0x90, 0x90, 0x90, 0x90,    // 11 - jmp $+11; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill16_12[12] =
        {0xeb, 0x0a, 0x90, 0x90, 0x90, 0x90,    // 12 - jmp $+12; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill16_13[13] =
        {0xeb, 0x0b, 0x90, 0x90, 0x90, 0x90,    // 13 - jmp $+13; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill16_14[14] =
        {0xeb, 0x0c, 0x90, 0x90, 0x90, 0x90,    // 14 - jmp $+14; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill16_15[15] =
        {0xeb, 0x0d, 0x90, 0x90, 0x90, 0x90,    // 15 - jmp $+15; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char *fill16[16] =
    {
        NULL,      fill16_1,  fill16_2,  fill16_3,
        fill16_4,  fill16_5,  fill16_6,  fill16_7,
        fill16_8,  fill16_9,  fill16_10, fill16_11,
        fill16_12, fill16_13, fill16_14, fill16_15
    };

    static const unsigned char fill32_1[1] =
        {0x90};                                 // 1 - nop
    static const unsigned char fill32_2[2] =
        {0x66, 0x90};                           // 2 - xchg ax, ax (o16 nop)
    static const unsigned char fill32_3[3] =
        {0x8d, 0x76, 0x00};                     // 3 - lea esi, [esi+byte 0]
    static const unsigned char fill32_4[4] =
        {0x8d, 0x74, 0x26, 0x00};               // 4 - lea esi, [esi*1+byte 0]
    static const unsigned char fill32_5[5] =
        {0x90,                                  // 5 - nop
         0x8d, 0x74, 0x26, 0x00};               //     lea esi, [esi*1+byte 0]
    static const unsigned char fill32_6[6] =
        {0x8d, 0xb6, 0x00, 0x00, 0x00, 0x00};   // 6 - lea esi, [esi+dword 0]
    static const unsigned char fill32_7[7] =
        {0x8d, 0xb4, 0x26, 0x00, 0x00, 0x00,    // 7 - lea esi, [esi*1+dword 0]
         0x00};
    static const unsigned char fill32_8[8] =
        {0x90,                                  // 8 - nop
         0x8d, 0xb4, 0x26, 0x00, 0x00, 0x00,    //     lea esi, [esi*1+dword 0]
         0x00};
#if 0
        // GAS uses these
    static const unsigned char fill32_9[9] =
        {0x89, 0xf6,                            // 9 - mov esi, esi
         0x8d, 0xbc, 0x27, 0x00, 0x00, 0x00,    //     lea edi, [edi*1+dword 0]
         0x00};
    static const unsigned char fill32_10[10] =
        {0x8d, 0x76, 0x00,                      // 10 - lea esi, [esi+byte 0]
         0x8d, 0xbc, 0x27, 0x00, 0x00, 0x00,    //      lea edi, [edi+dword 0]
         0x00};
    static const unsigned char fill32_11[11] =
        {0x8d, 0x74, 0x26, 0x00,                // 11 - lea esi, [esi*1+byte 0]
         0x8d, 0xbc, 0x27, 0x00, 0x00, 0x00,    //      lea edi, [edi*1+dword 0]
         0x00};
    static const unsigned char fill32_12[12] =
        {0x8d, 0xb6, 0x00, 0x00, 0x00, 0x00,    // 12 - lea esi, [esi+dword 0]
         0x8d, 0xbf, 0x00, 0x00, 0x00, 0x00};   //      lea edi, [edi+dword 0]
    static const unsigned char fill32_13[13] =
        {0x8d, 0xb6, 0x00, 0x00, 0x00, 0x00,    // 13 - lea esi, [esi+dword 0]
         0x8d, 0xbc, 0x27, 0x00, 0x00, 0x00,    //      lea edi, [edi*1+dword 0]
         0x00};
    static const unsigned char fill32_14[14] =
        {0x8d, 0xb4, 0x26, 0x00, 0x00, 0x00,    // 14 - lea esi, [esi*1+dword 0]
         0x00,
         0x8d, 0xbc, 0x27, 0x00, 0x00, 0x00,    //      lea edi, [edi*1+dword 0]
         0x00};
#else
        // But on newer processors, these are recommended
    static const unsigned char fill32_9[9] =
        {0xeb, 0x07, 0x90, 0x90, 0x90, 0x90,    // 9 - jmp $+9; nop fill
         0x90, 0x90, 0x90};
    static const unsigned char fill32_10[10] =
        {0xeb, 0x08, 0x90, 0x90, 0x90, 0x90,    // 10 - jmp $+10; nop fill
         0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill32_11[11] =
        {0xeb, 0x09, 0x90, 0x90, 0x90, 0x90,    // 11 - jmp $+11; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill32_12[12] =
        {0xeb, 0x0a, 0x90, 0x90, 0x90, 0x90,    // 12 - jmp $+12; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill32_13[13] =
        {0xeb, 0x0b, 0x90, 0x90, 0x90, 0x90,    // 13 - jmp $+13; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char fill32_14[14] =
        {0xeb, 0x0c, 0x90, 0x90, 0x90, 0x90,    // 14 - jmp $+14; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
#endif
    static const unsigned char fill32_15[15] =
        {0xeb, 0x0d, 0x90, 0x90, 0x90, 0x90,    // 15 - jmp $+15; nop fill
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    static const unsigned char *fill32[16] =
    {
        NULL,      fill32_1,  fill32_2,  fill32_3,
        fill32_4,  fill32_5,  fill32_6,  fill32_7,
        fill32_8,  fill32_9,  fill32_10, fill32_11,
        fill32_12, fill32_13, fill32_14, fill32_15
    };

    // Long form nops available on more recent Intel and AMD processors
    static const unsigned char fill32new_3[3] =     // 3 - nop(3)
        {0x0f, 0x1f, 0x00};
    static const unsigned char fill32new_4[4] =     // 4 - nop(4)
        {0x0f, 0x1f, 0x40, 0x00};
    static const unsigned char fill32new_5[5] =     // 5 - nop(5)
        {0x0f, 0x1f, 0x44, 0x00, 0x00};
    static const unsigned char fill32new_6[6] =     // 6 - nop(6)
        {0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00};
    static const unsigned char fill32new_7[7] =     // 7 - nop(7)
        {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00};
    static const unsigned char fill32new_8[8] =     // 8 - nop(8)
        {0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00,
         0x00};
    static const unsigned char fill32new_9[9] =     // 9 - nop(9)
        {0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00,
         0x00, 0x00};

    // Longer forms preferred by Intel use repeated o16 prefixes
    static const unsigned char fill32intel_10[10] = // 10 - o16; cs; nop
        {0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00,
         0x00, 0x00, 0x00};
    static const unsigned char fill32intel_11[11] = // 11 - 2x o16; cs; nop
        {0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00,
         0x00, 0x00, 0x00, 0x00};
    static const unsigned char fill32intel_12[12] = // 12 - 3x o16; cs; nop
        {0x66, 0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84,
         0x00, 0x00, 0x00, 0x00, 0x00};
    static const unsigned char fill32intel_13[13] = // 13 - 4x o16; cs; nop
        {0x66, 0x66, 0x66, 0x66, 0x2e, 0x0f, 0x1f,
         0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const unsigned char fill32intel_14[14] = // 14 - 5x o16; cs; nop
        {0x66, 0x66, 0x66, 0x66, 0x66, 0x2e, 0x0f,
         0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const unsigned char fill32intel_15[15] = // 15 - 6x o16; cs; nop
        {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x2e,
         0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};

    // Longer forms preferred by AMD use fewer o16 prefixes and no CS prefix;
    // Source: Software Optimisation Guide for AMD Family 10h
    // Processors 40546 revision 3.10 February 2009
    static const unsigned char fill32amd_10[10] =   // 10 - nop(10)
        {0x66, 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00,
         0x00, 0x00, 0x00};
    static const unsigned char fill32amd_11[11] =
        {0x0f, 0x1f, 0x44, 0x00, 0x00,              // 11 - nop(5)
         0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00};       //      nop(6)
    static const unsigned char fill32amd_12[12] =
        {0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00,        // 12 - nop(6)
         0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00};       //      nop(6)
    static const unsigned char fill32amd_13[13] =
        {0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00,        // 13 - nop(6)
         0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00}; //      nop(7)
    static const unsigned char fill32amd_14[14] =
        {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00,  // 14 - nop(7)
         0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00}; //      nop(7)
    static const unsigned char fill32amd_15[15] =
        {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00,        // 15 - nop(7)
         0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00}; //      nop(8)

    static const unsigned char *fill32_intel[16] =
    {
        NULL,           fill32_1,       fill32_2,       fill32new_3,
        fill32new_4,    fill32new_5,    fill32new_6,    fill32new_7,
        fill32new_8,    fill32new_9,    fill32intel_10, fill32intel_11,
        fill32intel_12, fill32intel_13, fill32intel_14, fill32intel_15
    };
    static const unsigned char *fill32_amd[16] =
    {
        NULL,           fill32_1,       fill32_2,       fill32new_3,
        fill32new_4,    fill32new_5,    fill32new_6,    fill32new_7,
        fill32new_8,    fill32new_9,    fill32amd_10,   fill32amd_11,
        fill32amd_12,   fill32amd_13,   fill32amd_14,   fill32amd_15
    };

    switch (m_mode_bits)
    {
        case 16:
            return fill16;
        case 32:
            if (m_nop == NOP_INTEL)
                return fill32_intel;
            else if (m_nop == NOP_AMD)
                return fill32_amd;
            else
                return fill32;
        case 64:
            // We know long nops are available in 64-bit mode; default to Intel
            // ones if unspecified (to match GAS behavior).
            if (m_nop == NOP_AMD)
                return fill32_amd;
            else
                return fill32_intel;
        default:
            throw ValueError(N_("Invalid mode_bits in x86_get_fill"));
    }
}

void
X86Arch::AddDirectives(Directives& dirs, const llvm::StringRef& parser)
{
    static const Directives::Init<X86Arch> nasm_dirs[] =
    {
        {"cpu",     &X86Arch::DirCpu, Directives::ARG_REQUIRED},
        {"bits",    &X86Arch::DirBits, Directives::ARG_REQUIRED},
    };
    static const Directives::Init<X86Arch> gas_dirs[] =
    {
        {".code16", &X86Arch::DirCode16, Directives::ANY},
        {".code32", &X86Arch::DirCode32, Directives::ANY},
        {".code64", &X86Arch::DirCode64, Directives::ANY},
    };

    if (String::NocaseEqual(parser, "nasm"))
        dirs.AddArray(this, nasm_dirs, NELEMS(nasm_dirs));
    else if (String::NocaseEqual(parser, "gas") ||
             String::NocaseEqual(parser, "gnu"))
        dirs.AddArray(this, gas_dirs, NELEMS(gas_dirs));
}


void
X86Arch::ToBytes(const llvm::APFloat& flt,
                 Bytes& bytes,
                 size_t valsize,
                 size_t shift,
                 int warn) const
{
    if (valsize != 32 && valsize != 64 && valsize != 80)
    {
        assert(false && "invalid floating point constant size");
        return;
    }

    Overwrite(bytes, flt, valsize, shift, false, warn);
}

void
X86Arch::ToBytes(const IntNum& intn,
                 Bytes& bytes,
                 size_t valsize,
                 int shift,
                 int warn) const
{
    Overwrite(bytes, intn, valsize, shift, false, warn);
}

std::auto_ptr<EffAddr>
X86Arch::CreateEffAddr(std::auto_ptr<Expr> e) const
{
    return std::auto_ptr<EffAddr>(new X86EffAddr(m_parser == PARSER_GAS, e));
}

void
DoRegister()
{
    RegisterModule<ArchModule, ArchModuleImpl<X86Arch> >("x86");
}

}}} // namespace yasm::arch::x86
