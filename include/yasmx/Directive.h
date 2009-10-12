#ifndef YASM_DIRECTIVE_H
#define YASM_DIRECTIVE_H
///
/// @file
/// @brief Directive interface.
///
/// @license
///  Copyright (C) 2001-2007  Peter Johnson
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
#include "clang/Basic/SourceLocation.h"
#include "yasmx/Config/export.h"
#include "yasmx/Config/functional.h"
#include "yasmx/Support/scoped_ptr.h"
#include "yasmx/NameValue.h"


namespace llvm { class StringRef; }

namespace yasm
{

class Object;

/// Directive information.  Helper class for passing all information about
/// a directive to a handler.
class DirectiveInfo
{
public:
    DirectiveInfo(Object& object, clang::SourceLocation source)
        : m_object(object), m_source(source)
    {}

    bool isObject(const Object& oth) const { return &m_object == &oth; }
    Object& getObject() { return m_object; }
    NameValues& getNameValues() { return m_namevals; }
    NameValues& getObjextNameValues() { return m_objext_namevals; }
    clang::SourceLocation getSource() const { return m_source; }

private:
    Object& m_object;               ///< object
    NameValues m_namevals;          ///< name/values
    NameValues m_objext_namevals;   ///< object format-specific name/values
    clang::SourceLocation m_source; ///< source location
};

/// Directive handler function.
/// @param info     directive information
/// @note The directive parameter are *not* constant; the callee (directive
///       handler) is free to modify it (specifically the name/values
///       portions).  The typical modification performed is to swap or
///       otherwise remove values without copying.
typedef FUNCTION::function<void (DirectiveInfo& info)> Directive;

/// Container to manage and call directive handlers.
class YASM_LIB_EXPORT Directives
{
public:
    /// Tests to perform prior to directive handler being called.
    /// These can be used to simplify a directive function implementation.
    enum Flags
    {
        ANY = 0,            ///< Any valparams accepted
        ARG_REQUIRED = 1,   ///< Require at least 1 valparam
        ID_REQUIRED = 2     ///< First valparam must be ID
    };

    template <typename T>
    struct Init
    {
        const char* name;
        void (T::*func) (DirectiveInfo& info);
        Flags flags;
    };

    Directives();
    ~Directives();

    /// Add a directive.
    /// @param name         Directive name.  GAS directives should include
    ///                     the ".", NASM directives should just be the raw
    ///                     name (not including the []).
    /// @param handler      Directive function
    /// @param flags        Flags for pre-handler parameter checking.
    void Add(const llvm::StringRef& name, Directive handler, Flags flags = ANY);

    /// Add directives from an initializer array.
    /// @param me           this pointer to associate
    /// @param inits        initializer array
    /// @param size         size of initializer array
    template <typename T>
    void AddArray(T* me, const Init<T>* inits, unsigned int size)
    {
        for (unsigned int i=0; i<size; ++i)
            Add(inits[i].name,
                BIND::bind(inits[i].func, me, _1),
                inits[i].flags);
    }

    /// Get a directive functor.  Throws an exception if no match.
    /// @param name         directive name
    Directive operator[] (const llvm::StringRef& name) const;

    /// Get a directive functor.  Returns false if no match.
    /// @param handler      directive handler (returned)
    /// @param name         directive name
    /// @return True if directive exists, and handler is set to the
    ///         matching handler.
    bool get(Directive* handler, const llvm::StringRef& name) const;

private:
    /// Pimpl for class internals.
    class Impl;
    util::scoped_ptr<Impl> m_impl;
};

} // namespace yasm

#endif
