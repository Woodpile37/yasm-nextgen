#ifndef YASM_ELFOBJECT_H
#define YASM_ELFOBJECT_H
//
// ELF object format
//
//  Copyright (C) 2003-2007  Michael Urman
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
// Notes
//
// elf-objfmt uses the "linking" view of an ELF file:
// ELF header, an optional program header table, several sections,
// and a section header table
//
// The ELF header tells us some overall program information,
//   where to find the PHT (if it exists) with phnum and phentsize,
//   and where to find the SHT with shnum and shentsize
//
// The PHT doesn't seem to be generated by NASM for elftest.asm
//
// The SHT
//
// Each Section is spatially disjoint, and has exactly one SHT entry.
//
#include "llvm/ADT/StringMap.h"
#include "yasmx/Support/ptr_vector.h"
#include "yasmx/Support/scoped_ptr.h"
#include "yasmx/ObjectFormat.h"

#include "ElfConfig.h"


namespace yasm
{
class DiagnosticsEngine;
class DirectiveInfo;

namespace objfmt
{
class ElfMachine;
class ElfSection;
class ElfSymbol;

// ELF symbol version alias.
class YASM_STD_EXPORT ElfSymVersion
{
public:
    enum Mode
    {
        Standard,   // name@node in gas syntax
        Default,    // name@@node in gas syntax (default version)
        Auto        // name@@@node in gas syntax (automatic std/default)
    };

    ElfSymVersion(llvm::StringRef real,
                  llvm::StringRef name,
                  llvm::StringRef version,
                  Mode mode)
        : m_real(real)
        , m_name(name)
        , m_version(version)
        , m_mode(mode)
    {
    }

    std::string m_real;
    std::string m_name;
    std::string m_version;
    Mode m_mode;
};

class YASM_STD_EXPORT ElfGroup
{
public:
    ElfGroup();
    ~ElfGroup();

    unsigned long flags;
    std::string name;
    std::vector<Section*> sects;
    util::scoped_ptr<ElfSection> elfsect;
    SymbolRef sym;
};

class YASM_STD_EXPORT ElfObject : public ObjectFormat
{
public:
    ElfObject(const ObjectFormatModule& module,
              Object& object,
              unsigned int bits=0);
    ~ElfObject();

    static llvm::StringRef getName() { return "ELF"; }
    static llvm::StringRef getKeyword() { return "elf"; }
    static llvm::StringRef getExtension() { return ".o"; }
    static unsigned int getDefaultX86ModeBits() { return 0; }
    static llvm::StringRef getDefaultDebugFormatKeyword() { return "cfi"; }
    static std::vector<llvm::StringRef> getDebugFormatKeywords();
    static bool isOkObject(Object& object) { return true; }
    static bool Taste(const llvm::MemoryBuffer& in,
                      /*@out@*/ std::string* arch_keyword,
                      /*@out@*/ std::string* machine)
    { return false; }

    void AddDirectives(Directives& dirs, llvm::StringRef parser);

    void InitSymbols(llvm::StringRef parser);

    bool Read(SourceManager& sm, DiagnosticsEngine& diags);
    void Output(llvm::raw_fd_ostream& os,
                bool all_syms,
                DebugFormat& dbgfmt,
                DiagnosticsEngine& diags);

    Section* AddDefaultSection();
    Section* AppendSection(llvm::StringRef name,
                           SourceLocation source,
                           DiagnosticsEngine& diags);

    ElfSymbol& BuildSymbol(Symbol& sym);
    void BuildExtern(Symbol& sym, DiagnosticsEngine& diags);
    void BuildGlobal(Symbol& sym, DiagnosticsEngine& diags);
    void BuildCommon(Symbol& sym, DiagnosticsEngine& diags);
    void setSymbolSectionValue(Symbol& sym, ElfSymbol& elfsym);
    void FinalizeSymbol(Symbol& sym,
                        StringTable& strtab,
                        bool local_names,
                        DiagnosticsEngine& diags);

    void DirGasSection(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirSection(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirType(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirSize(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirWeak(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirWeakRef(DirectiveInfo& info, DiagnosticsEngine& diags);
    void VisibilityDir(DirectiveInfo& info,
                       DiagnosticsEngine& diags,
                       ElfSymbolVis vis);
    void DirInternal(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirHidden(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirProtected(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirSymVer(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirIdent(DirectiveInfo& info, DiagnosticsEngine& diags);
    void DirVersion(DirectiveInfo& info, DiagnosticsEngine& diags);

    ElfConfig m_config;                     // ELF configuration
    util::scoped_ptr<ElfMachine> m_machine; // ELF machine interface

    ElfSymbol* m_file_elfsym;               // .file symbol
    SymbolRef m_dotdotsym;                  // ..sym symbol

    typedef stdx::ptr_vector<ElfSymVersion> SymVers;
    SymVers m_symvers;                      // symbol version aliases
    stdx::ptr_vector_owner<ElfSymVersion> m_symvers_owner;

    typedef stdx::ptr_vector<ElfGroup> Groups;
    Groups m_groups;                        // section groups
    stdx::ptr_vector_owner<ElfGroup> m_groups_owner;

    llvm::StringMap<ElfGroup*> m_group_map; // section groups by name
};

class YASM_STD_EXPORT Elf32Object : public ElfObject
{
public:
    Elf32Object(const ObjectFormatModule& module, Object& object)
        : ElfObject(module, object, 32)
    {}
    ~Elf32Object();

    static llvm::StringRef getName() { return "ELF (32-bit)"; }
    static llvm::StringRef getKeyword() { return "elf32"; }
    static llvm::StringRef getExtension() { return ElfObject::getExtension(); }
    static unsigned int getDefaultX86ModeBits() { return 32; }

    static llvm::StringRef getDefaultDebugFormatKeyword()
    { return ElfObject::getDefaultDebugFormatKeyword(); }
    static std::vector<llvm::StringRef> getDebugFormatKeywords()
    { return ElfObject::getDebugFormatKeywords(); }

    static bool isOkObject(Object& object);

    // For tasting, let main elf handle it.
    static bool Taste(const llvm::MemoryBuffer& in,
                      /*@out@*/ std::string* arch_keyword,
                      /*@out@*/ std::string* machine);
};

class YASM_STD_EXPORT Elf64Object : public ElfObject
{
public:
    Elf64Object(const ObjectFormatModule& module, Object& object)
        : ElfObject(module, object, 64)
    {}
    ~Elf64Object();

    static llvm::StringRef getName() { return "ELF (64-bit)"; }
    static llvm::StringRef getKeyword() { return "elf64"; }
    static llvm::StringRef getExtension() { return ElfObject::getExtension(); }
    static unsigned int getDefaultX86ModeBits() { return 64; }

    static llvm::StringRef getDefaultDebugFormatKeyword()
    { return ElfObject::getDefaultDebugFormatKeyword(); }
    static std::vector<llvm::StringRef> getDebugFormatKeywords()
    { return ElfObject::getDebugFormatKeywords(); }

    static bool isOkObject(Object& object);

    // For tasting, let main elf handle it.
    static bool Taste(const llvm::MemoryBuffer& in,
                      /*@out@*/ std::string* arch_keyword,
                      /*@out@*/ std::string* machine);
};

class YASM_STD_EXPORT Elfx32Object : public ElfObject
{
public:
    Elfx32Object(const ObjectFormatModule& module, Object& object)
        : ElfObject(module, object, 32)
    {}
    ~Elfx32Object();

    static llvm::StringRef getName() { return "ELF (x32)"; }
    static llvm::StringRef getKeyword() { return "elfx32"; }
    static llvm::StringRef getExtension() { return ElfObject::getExtension(); }
    static unsigned int getDefaultX86ModeBits() { return 64; }

    static llvm::StringRef getDefaultDebugFormatKeyword()
    { return ElfObject::getDefaultDebugFormatKeyword(); }
    static std::vector<llvm::StringRef> getDebugFormatKeywords()
    { return ElfObject::getDebugFormatKeywords(); }

    static bool isOkObject(Object& object);

    // For tasting, let main elf handle it.
    static bool Taste(const llvm::MemoryBuffer& in,
                      /*@out@*/ std::string* arch_keyword,
                      /*@out@*/ std::string* machine);
};

}} // namespace yasm::objfmt

#endif
