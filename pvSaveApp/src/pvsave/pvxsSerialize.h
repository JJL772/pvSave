
#pragma once

#include "pvxs/data.h"

namespace pvsave {

    const char* ntTypeString(const pvxs::Value& value);

    void ntToString(const pvxs::Value& value, char* buf, size_t bufLen);

    bool ntScalarToString(const pvxs::Value& value, char* buf, size_t bufLen);

    std::pair<bool, pvxs::TypeCode> ntTypeFromString(const char* ptype);

    std::pair<bool, pvxs::Value> ntScalarFromString(const char* pval, pvxs::TypeCode type);

}