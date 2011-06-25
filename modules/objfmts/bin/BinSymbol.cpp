//
// Flat-format binary object format symbol data
//
//  Copyright (C) 2002-2008  Peter Johnson
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
#include "BinSymbol.h"

#include "llvm/ADT/Twine.h"
#include "yasmx/BytecodeContainer.h"
#include "yasmx/Bytecode.h"
#include "yasmx/Expr.h"
#include "yasmx/Section.h"
#include "yasmx/Symbol.h"

#include "BinSection.h"


using namespace yasm;
using namespace yasm::objfmt;

const char* BinSymbol::key = "objfmt::bin::BinSymbol";

BinSymbol::BinSymbol(const Section& sect,
                     const BinSection& bsd,
                     SpecialSym which)
    : m_sect(sect), m_bsd(bsd), m_which(which)
{
}

BinSymbol::~BinSymbol()
{
}

#ifdef WITH_XML
pugi::xml_node
BinSymbol::Write(pugi::xml_node out) const
{
    pugi::xml_node root = out.append_child("BinSymbol");
    root.append_attribute("key") = key;
    root.append_attribute("section") = m_sect.getName().str().c_str();
    pugi::xml_attribute ssym = root.append_attribute("ssym");
    switch (m_which)
    {
        case START:     ssym = "START"; break;
        case VSTART:    ssym = "VSTART"; break;
        case LENGTH:    ssym = "LENGTH"; break;
    }
    return root;
}
#endif // WITH_XML

bool
BinSymbol::getValue(IntNum* val) const
{
    switch (m_which)
    {
        case START:
            if (!m_bsd.has_istart)
                return false;
            *val = m_sect.getLMA();
            return true;
        case VSTART:
            if (!m_bsd.has_ivstart)
                return false;
            *val = m_sect.getVMA();
            return true;
        case LENGTH:
            if (!m_bsd.has_length)
                return false;
            *val = m_bsd.length;
            return true;
        default:
            assert(false);
            return false;
    }
}

void
objfmt::BinSimplify(Expr& e)
{
    for (ExprTerms::iterator i=e.getTerms().begin(),
         end=e.getTerms().end(); i != end; ++i)
    {
        Symbol* sym;

        // Transform our special symrecs into the appropriate value
        IntNum ssymval;
        if ((sym = i->getSymbol()) && getBinSSymValue(*sym, &ssymval))
            i->setIntNum(ssymval);

        // Transform symrecs or precbcs that reference sections into
        // vstart + intnum(dist).
        Location loc;
        if ((sym = i->getSymbol()) && sym->getLabel(&loc))
            ;
        else if (Location* locp = i->getLocation())
            loc = *locp;
        else
            continue;

        BytecodeContainer* container = loc.bc->getContainer();
        Location first = {&container->bytecodes_front(), 0};
        IntNum dist;
        if (CalcDist(first, loc, &dist))
        {
            const Section* sect = container->getSection();
            dist += sect->getVMA();
            i->setIntNum(dist);
        }
    }
}
