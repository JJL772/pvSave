
#pragma once

#include "pvSave.h"

#include "dbFldTypes.h"

#include "pvsave/variant.h"

namespace pvsave {

    /**
     * \brief Return human-readable string for the field type
     * \param ftype Field type
     * \returns String for the field type or ""
     */
    const char* dbTypeString(dbfType ftype);

    /**
     * \brief Returns a dbfType based on the provided string
     * \param str Field type string
     * \returns dbfType for the string, or DBF_NOACCESS if invalid
     */
    dbfType dbTypeFromString(const char* str);

    /**
     * \brief Convert a ETypeCode to string
     */
    const char* typeCodeString(ETypeCode code);

    /**
     * \brief Convert a string to an ETypeCode
     */
    std::pair<bool, ETypeCode> typeCodeFromString(const char* str);

    const char* parseString(const char* pstr, std::string& out);

    /**
     * \brief Helper to pad out to an indent level
     * \param fp Stream to print to
     * \param indent Indentation level to pad to. This is measured in spaces (8 = 8 spaces total)
     */
    void pindent(FILE* fp, int indent);

    std::pair<bool, Data> dataParseString(const char* pstring, ETypeCode expected);

    bool dataToString(const Data& data, char* outBuf, size_t bufLen);
}