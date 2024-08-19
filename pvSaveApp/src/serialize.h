
#pragma once

#include "pvxs/data.h"

#include "variant.hpp"

namespace pvsave {

    using Data = Variant<int64_t, uint64_t, double, std::string>;

    const char* ntTypeString(const pvxs::Value& value);

    void ntToString(const pvxs::Value& value, char* buf, size_t bufLen);

    bool ntScalarToString(const pvxs::Value& value, char* buf, size_t bufLen);

    std::pair<bool, pvxs::TypeCode> ntTypeFromString(const char* ptype);

    std::pair<bool, pvxs::Value> ntScalarFromString(const char* pval, pvxs::TypeCode type);

    const char* parseString(const char* pstr, std::string& out);

    /**
     * \brief Helper to pad out to an indent level
     * \param fp Stream to print to
     * \param indent Indentation level to pad to. This is measured in spaces (8 = 8 spaces total)
     */
    void pindent(FILE* fp, int indent);
}