#ifndef YASM_SYMBOL_H
#define YASM_SYMBOL_H
///
/// @file
/// @brief Symbol interface.
///
/// @license
///  Copyright (C) 2001-2007  Michael Urman, Peter Johnson
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
///  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
/// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
/// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
/// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
/// POSSIBILITY OF SUCH DAMAGE.
/// @endlicense
///
#include <string>

#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/StringRef.h"
#include "yasmx/Config/export.h"
#include "yasmx/Support/scoped_ptr.h"
#include "yasmx/AssocData.h"
#include "yasmx/Location.h"


namespace YAML { class Emitter; }

namespace yasm
{

class Expr;

/// A symbol.
class YASM_LIB_EXPORT Symbol : public AssocDataContainer
{
public:
    /// Constructor.
    explicit Symbol(const llvm::StringRef& name);

    /// Destructor.
    ~Symbol();

    /// Symbol status.  #DEFINED is set by define_label(), define_equ(), or
    /// declare(), with a visibility of #EXTERN or #COMMON.
    enum Status
    {
        NOSTATUS = 0,           ///< no status
        USED = 1 << 0,          ///< for use before definition
        DEFINED = 1 << 1,       ///< once it's been defined in the file
        VALUED = 1 << 2         ///< once its value has been determined
    };

    /// Symbol record visibility.
    /// @note #EXTERN and #COMMON are mutually exclusive.
    enum Visibility
    {
        LOCAL = 0,              ///< Default, local only
        GLOBAL = 1 << 0,        ///< If symbol is declared GLOBAL
        COMMON = 1 << 1,        ///< If symbol is declared COMMON
        EXTERN = 1 << 2,        ///< If symbol is declared EXTERN
        DLOCAL = 1 << 3         ///< If symbol is explicitly declared LOCAL
    };

    /// Get the name of a symbol.
    /// @return Symbol name.
    llvm::StringRef getName() const { return m_name; }

    /// Get the visibility of a symbol.
    /// @return Symbol visibility.
    int getVisibility() const { return m_visibility; }

    /// Get the status of a symbol.
    /// @return Symbol status.
    int getStatus() const { return m_status; }

    /// Get the source location where a symbol was first defined.
    /// @return Source location.
    clang::SourceLocation getDefSource() const { return m_def_source; }

    /// Get the source location where a symbol was first declared.
    /// @return Source location.
    clang::SourceLocation getDeclSource() const { return m_decl_source; }

    /// Get the source location where a symbol was first used.
    /// @return Source location.
    clang::SourceLocation getUseSource() const { return m_use_source; }

    /// Get EQU value of a symbol.
    /// @return EQU value, or NULL if symbol is not an EQU or is not defined.
    /*@null@*/ const Expr* getEqu() const;

    /// Get the label location of a symbol.
    /// @param sym       symbol
    /// @param loc       label location (output)
    /// @return False if not symbol is not a label or if the symbol's
    ///         visibility is #EXTERN or #COMMON (not defined in the file).
    bool getLabel(/*@out@*/ Location* loc) const;

    /// Determine if symbol is the "absolute" symbol created by
    /// yasm_symtab_abs_sym().
    /// @return False if symbol is not the "absolute" symbol, true otherwise.
    bool isAbsoluteSymbol() const
    {
        return !m_def_source.isValid() && m_type == EQU && m_name.empty();
    }

    /// Determine if symbol is a special symbol.
    /// @return False if symbol is not a special symbol, true otherwise.
    bool isSpecial() const { return m_type == SPECIAL; }

    /// Mark the symbol as used.  The symbol does not necessarily need to
    /// be defined before it is used.
    /// @param source   source location
    void Use(clang::SourceLocation source);

    /// Define as an EQU value.
    /// @param e        EQU value (expression)
    /// @param source   source location
    void DefineEqu(const Expr& e,
                   clang::SourceLocation source = clang::SourceLocation());

    /// Define as a label.
    /// @param loc      location of label
    /// @param source   source location
    void DefineLabel(Location loc,
                     clang::SourceLocation source = clang::SourceLocation());

    /// Define a special symbol.  Special symbols have no generic associated
    /// data (such as an expression or precbc).
    /// @param vis      symbol visibility
    /// @param source   source location
    void DefineSpecial(Symbol::Visibility vis,
                       clang::SourceLocation source = clang::SourceLocation());

    /// Declare external visibility.
    /// @note Not all visibility combinations are allowed.
    /// @param vis      visibility
    /// @param source   source location
    void Declare(Visibility vis,
                 clang::SourceLocation source = clang::SourceLocation());

    /// Finalize symbol after parsing stage.  Errors on symbols that
    /// are used but never defined or declared #EXTERN or #COMMON.
    /// @param undef_extern if true, all undef syms should be declared extern
    void Finalize(bool undef_extern);

    /// Write a YAML representation.  For debugging purposes.
    /// @param out          YAML emitter
    void Write(YAML::Emitter& out) const;

    /// Dump a YAML representation to stderr.
    /// For debugging purposes.
    void Dump() const;

private:
    Symbol(const Symbol&);                  // not implemented
    const Symbol& operator=(const Symbol&); // not implemented

    enum Type
    {
        UNKNOWN,    ///< for unknown type (COMMON/EXTERN)
        EQU,        ///< for EQU defined symbols (expressions)
        LABEL,      ///< for labels

        /// For special symbols that need to be in the symbol table but
        /// otherwise have no purpose.
        SPECIAL
    };

    void Define(Type type, clang::SourceLocation source);

    std::string m_name;
    Type m_type;
    int m_status;
    int m_visibility;
    clang::SourceLocation m_def_source;     ///< where symbol was first defined
    clang::SourceLocation m_decl_source;    ///< where symbol was first declared
    clang::SourceLocation m_use_source;     ///< where symbol was first used

    // Possible data

    util::scoped_ptr<Expr> m_equ;   ///< EQU value

    /// Label location
    Location m_loc;
};

inline void
Symbol::Use(clang::SourceLocation source)
{
    if (!m_use_source.isValid())
        m_use_source = source;      // set source location of first use
    m_status |= USED;
}

inline const Expr*
Symbol::getEqu() const
{
    if (m_type == EQU && (m_status & VALUED))
        return m_equ.get();
    return 0;
}

/// Dump a YAML representation of a symbol.  For debugging purposes.
/// @param out          YAML emitter
/// @param symbol       symbol
/// @return Emitter.
inline YAML::Emitter&
operator<< (YAML::Emitter& out, const Symbol& symbol)
{
    symbol.Write(out);
    return out;
}

} // namespace yasm

#endif
