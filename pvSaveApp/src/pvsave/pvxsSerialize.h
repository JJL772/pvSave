//////////////////////////////////////////////////////////////////////////////
// This file is part of 'pvSave'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'pvSave', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pvxs/data.h"

namespace pvsave {

    const char* ntTypeString(const pvxs::Value& value);

    void ntToString(const pvxs::Value& value, char* buf, size_t bufLen);

    bool ntScalarToString(const pvxs::Value& value, char* buf, size_t bufLen);

    std::pair<bool, pvxs::TypeCode> ntTypeFromString(const char* ptype);

    std::pair<bool, pvxs::Value> ntScalarFromString(const char* pval, pvxs::TypeCode type);

}