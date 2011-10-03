#ifndef YASM_X86COMMON_H
#define YASM_X86COMMON_H
//
// x86 common instruction information interface
//
//  Copyright (C) 2001-2007  Peter Johnson
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
//
#include <vector>

#include "yasmx/Config/export.h"
#include "yasmx/DebugDumper.h"
#include "yasmx/Insn.h"


namespace yasm
{
namespace arch
{

class X86SegmentRegister;

class YASM_STD_EXPORT X86Common
{
public:
    X86Common();

    void ApplyPrefixes(unsigned int def_opersize_64,
                       const Insn::Prefixes& prefixes,
                       DiagnosticsEngine& diags,
                       unsigned char* rex = 0);
    void Finish();

    unsigned long getLen() const;
    void ToBytes(Bytes& bytes, const X86SegmentRegister* segreg) const;

#ifdef WITH_XML
    pugi::xml_node Write(pugi::xml_node out) const;
#endif // WITH_XML

    unsigned char m_addrsize;       // 0 or =mode_bits => no override
    unsigned char m_opersize;       // 0 or =mode_bits => no override
    unsigned char m_lockrep_pre;    // 0 indicates no prefix

    // We need this because xacquire/xrelease might require F0 prefix.
    unsigned char m_acqrel_pre;     // 0 indicates no prefix

    unsigned char m_mode_bits;
};

}} // namespace yasm::arch

#endif
