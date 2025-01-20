//////////////////////////////////////////////////////////////////////////////
// This file is part of 'pvSave'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'pvSave', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////

#include "pvsave/pvSave.h"

pvsave::pvSaveIO::pvSaveIO(const char* instName) :
    instName_(instName)
{
    pvsave::ioBackends().insert({instName, this});
}

pvsave::pvSaveIO::~pvSaveIO()
{
    auto it = pvsave::ioBackends().find(instName_);
    if (it != pvsave::ioBackends().end())
        pvsave::ioBackends().erase(it);
}
