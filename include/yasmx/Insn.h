#ifndef YASM_INSN_H
#define YASM_INSN_H
///
/// @file
/// @brief Mnenomic instructions interface.
///
/// @license
///  Copyright (C) 2002-2007  Peter Johnson
///
/// Redistribution and use in source and binary forms, with or without
/// modification, are permitted provided that the following conditions
/// are met:
///  - Redistributions of source code must retain the above copyright
///    notice, this list of conditions and the following disclaimer.
///  - Redistributions in binary form must reproduce the above copyright
///    notice, this list of conditions and the following disclaimer in the
///    documentation and/or other materials provided with the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
/// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
/// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
/// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
/// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
/// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
/// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
/// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
/// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
/// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
/// POSSIBILITY OF SUCH DAMAGE.
/// @endlicense
///
#include <memory>
#include <vector>

#include "llvm/ADT/SmallVector.h"
#include "yasmx/Config/export.h"
#include "yasmx/EffAddr.h"
#include "yasmx/Expr.h"


namespace clang { class SourceLocation; }
namespace YAML { class Emitter; class raw_ostream; }

namespace yasm
{

class BytecodeContainer;
class Bytes;
class Register;
class SegmentRegister;

/// Base class for target modifiers.
class YASM_LIB_EXPORT TargetModifier
{
public:
    TargetModifier() {}
    virtual ~TargetModifier();
    virtual void Put(llvm::raw_ostream& os) const = 0;

    /// Write a YAML representation.  For debugging purposes.
    /// @param out          YAML emitter
    virtual void Write(YAML::Emitter& out) const = 0;

    /// Dump a YAML representation to stderr.
    /// For debugging purposes.
    void Dump() const;

private:
    // not implemented (noncopyable class)
    TargetModifier(const TargetModifier&);
    const TargetModifier& operator=(const TargetModifier&);
};

/// An instruction operand.
class YASM_LIB_EXPORT Operand
{
public:
    /// Operand type.
    enum Type
    {
        NONE = 0,   ///< Nothing.
        REG,        ///< A register.
        SEGREG,     ///< A segment register.
        MEMORY,     ///< An effective address (memory reference).
        IMM         ///< An immediate or jump target.
    };

    /// Create an instruction operand from a register.
    /// @param reg  register
    Operand(const Register* reg);

    /// Create an instruction operand from a segment register.
    /// @param segreg       segment register
    Operand(const SegmentRegister* segreg);

    /// Create an instruction operand from an effective address.
    /// @param ea   effective address
    Operand(std::auto_ptr<EffAddr> ea);

    /// Create an instruction operand from an immediate expression.
    /// Looks for cases of a single register and creates a register
    /// variant.
    /// @param val  immediate expression
    /// @return Newly allocated operand.
    Operand(std::auto_ptr<Expr> val);

    /// Explicit copy creator.
    /// There's no copy constructor or assignment operator as we want
    /// to use the default bit-copy ones.  Even though this class
    /// contains more complex structures, we don't want to be copying
    /// the contents all the time.
    Operand clone() const;

    /// Explicit release.
    /// Doesn't destroy contents, just ensures what contents are there
    /// won't be destroyed via the destructor.
    void Release()
    {
        m_type = NONE;
        m_reg = 0;
    }

    /// Explicit destructor.
    /// Similar to clone(), we do smart copying and destruction in
    /// #Expr implementation to prevent over-copying of possibly deep
    /// expression trees.
    void Destroy();

    void Finalize();

    /// Match type.
    bool isType(Type type) const
    { return m_type == static_cast<unsigned int>(type); }

    /// Get the type.
    Type getType() const { return static_cast<Type>(m_type); }

    /// Helper functions to get specific types.

    const Register* getReg() const
    {
        return (m_type == REG ? m_reg : 0);
    }

    const SegmentRegister* getSegReg() const
    {
        return (m_type == SEGREG ? m_segreg : 0);
    }

    EffAddr* getMemory() const
    {
        return (m_type == MEMORY ? m_ea : 0);
    }

    Expr* getImm() const
    {
        return (m_type == IMM ? m_val : 0);
    }

    /// Helper functions to release specific types
    std::auto_ptr<EffAddr> ReleaseMemory();
    std::auto_ptr<Expr> ReleaseImm();

    /// Release segment expression, 0 if none.
    std::auto_ptr<Expr> ReleaseSeg();

    /// Get segment expression, 0 if none.
    Expr* getSeg() { return m_seg; }
    const Expr* getSeg() const { return m_seg; }

    /// Set segment expression.
    void setSeg(std::auto_ptr<Expr> seg);

    /// Get arch target modifier, 0 if none.
    const TargetModifier* getTargetMod() const { return m_targetmod; }

    /// Set target modifier.
    void setTargetMod(const TargetModifier* tmod) { m_targetmod = tmod; }

    /// Get operand size, in bits.  0 if not user specified.
    unsigned int getSize() const { return m_size; }

    /// Set operand size, in bits.
    void setSize(unsigned int size) { m_size = size; }

    /// Is the operand dereferenced, as in "*foo" in GAS?
    bool isDeref() const { return m_deref; }

    /// Set whether the operand is dereferenced.
    void setDeref(bool deref=true) { m_deref = deref; }

    /// Is the operand strict, as in "strict foo" in NASM?
    bool isStrict() const { return m_strict; }

    /// Set the strictness of the operand.
    void setStrict(bool strict=true) { m_strict = strict; }

    /// Write a YAML representation.  For debugging purposes.
    /// @param out          YAML emitter
    void Write(YAML::Emitter& out) const;

    /// Dump a YAML representation to stderr.
    /// For debugging purposes.
    void Dump() const;

private:
    /// Operand data.
    union
    {
        const SegmentRegister* m_segreg;    ///< Segment register.
        const Register* m_reg;              ///< Register.
        EffAddr* m_ea;      ///< Effective address for memory references.
        Expr* m_val;        ///< Value of immediate or jump target.
    };

    /// Segment expression.
    Expr* m_seg;

    /// Arch target modifier, 0 if none.
    const TargetModifier* m_targetmod;

    /// Specified size of the operand, in bits.
    /// 0 if not user-specified.
    unsigned int m_size:16;

    /// Nonzero if dereference.  Used for "*foo" in GAS.
    /// The reason for this is that by default in GAS, an unprefixed value
    /// is a memory address, except for jumps/calls, in which case it
    /// needs a "*" prefix to become a memory address (otherwise it's an
    /// immediate).  This isn't knowable in the parser stage, so the
    /// parser sets this flag to indicate the "*" prefix has been used,
    /// and the arch needs to adjust the operand type appropriately
    /// depending on the instruction type.
    unsigned int m_deref:1;

    /// Nonzero if strict.  Used for "strict foo" in NASM.
    /// This is used to inhibit optimization on otherwise "sized" values.
    /// For example, the user may just want to be explicit with the size
    /// on "push dword 4", but not actually want to force the immediate
    /// size to 4 bytes (rather wanting the optimizer to optimize it down
    /// to 1 byte as though "dword" was not specified).  To indicate the
    /// immediate should actually be forced to 4 bytes, the user needs to
    /// write "push strict dword 4", which sets this flag.
    unsigned int m_strict:1;

    /// Operand type.
    unsigned int m_type:4;
};

/// Base class for instruction prefixes.
class YASM_LIB_EXPORT Prefix
{
public:
    Prefix() {}
    virtual ~Prefix();
    virtual void Put(llvm::raw_ostream& os) const = 0;

    /// Write a YAML representation.  For debugging purposes.
    /// @param out          YAML emitter
    virtual void Write(YAML::Emitter& out) const = 0;

    /// Dump a YAML representation to stderr.
    /// For debugging purposes.
    void Dump() const;

private:
    Prefix(const Prefix&);                  // not implemented
    const Prefix& operator=(const Prefix&); // not implemented
};

/// Base class for instructions.  Architectures should
/// derive their own implementation from this.
class YASM_LIB_EXPORT Insn
{
public:
    typedef std::auto_ptr<Insn> Ptr;

    typedef llvm::SmallVector<Operand, 3> Operands;
    typedef std::vector<const Prefix*> Prefixes;
    typedef std::vector<const SegmentRegister*> SegRegs;

    Insn();
    virtual ~Insn();

    /// Add operand to the end of an instruction.
    /// @param op           operand
    void AddOperand(const Operand& op)
    {
        m_operands.push_back(op);
    }

    /// Associate a prefix with an instruction.
    /// @param prefix       data that identifies the prefix
    void AddPrefix(const Prefix* prefix)
    {
        m_prefixes.push_back(prefix);
    }

    /// Associate a segment prefix with an instruction.
    /// @param segreg       data that identifies the segment register
    void AddSegPrefix(const SegmentRegister* segreg)
    {
        m_segregs.push_back(segreg);
    }

    /// Append instruction to a bytecode container.
    void Append(BytecodeContainer& container, clang::SourceLocation source);

    virtual Insn* clone() const = 0;

    /// Write a YAML representation.  For debugging purposes.
    /// @param out          YAML emitter
    void Write(YAML::Emitter& out) const;

    /// Dump a YAML representation to stderr.
    /// For debugging purposes.
    void Dump() const;

protected:
    /// Copy constructor.
    Insn(const Insn& rhs);

    /// Append instruction to a section.
    virtual void DoAppend(BytecodeContainer& container,
                          clang::SourceLocation source) = 0;

    /// Write derived class YAML representation.  For debugging purposes.
    /// @param out          YAML emitter
    virtual void DoWrite(YAML::Emitter& out) const = 0;

    /// Operands.
    Operands m_operands;

    /// Prefixes.
    Prefixes m_prefixes;

    /// Array of segment prefixes.
    SegRegs m_segregs;

private:
    const Insn& operator=(const Insn&);
};

inline llvm::raw_ostream&
operator<< (llvm::raw_ostream& os, const TargetModifier& tmod)
{
    tmod.Put(os);
    return os;
}

inline llvm::raw_ostream&
operator<< (llvm::raw_ostream& os, const Prefix& prefix)
{
    prefix.Put(os);
    return os;
}

/// Dump a YAML representation of a target modifier.  For debugging purposes.
/// @param out          YAML emitter
/// @param tmod         target modifier
/// @return Emitter.
inline YAML::Emitter&
operator<< (YAML::Emitter& out, const TargetModifier& tmod)
{
    tmod.Write(out);
    return out;
}

/// Dump a YAML representation of a prefix.  For debugging purposes.
/// @param out          YAML emitter
/// @param prefix       prefix
/// @return Emitter.
inline YAML::Emitter&
operator<< (YAML::Emitter& out, const Prefix& prefix)
{
    prefix.Write(out);
    return out;
}

/// Dump a YAML representation of an operand.  For debugging purposes.
/// @param out          YAML emitter
/// @param operand      operand
/// @return Emitter.
inline YAML::Emitter&
operator<< (YAML::Emitter& out, const Operand& operand)
{
    operand.Write(out);
    return out;
}

/// Dump a YAML representation of an instruction.  For debugging purposes.
/// @param out          YAML emitter
/// @param insn         instruction
/// @return Emitter.
inline YAML::Emitter&
operator<< (YAML::Emitter& out, const Insn& insn)
{
    insn.Write(out);
    return out;
}

} // namespace yasm

#endif
