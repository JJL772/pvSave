
#include <stdlib.h>
#include <string.h>
#include <unordered_map>

#include "epicsStdlib.h"
#include "epicsAssert.h"

#include "pvsave/serialize.h"

#include "pvxs/nt.h"

bool pvsave::ntScalarToString(const pvxs::Value &value, char *buf, size_t bufLen) {
    auto sub = value["value"];
    if (!sub || sub.type().kind() == pvxs::Kind::Compound) {
        return false;
    }

    switch (sub.type().kind()) {
    case pvxs::Kind::Bool:
        snprintf(buf, bufLen, sub.as<bool>() ? "1" : "0");
        break;
    case pvxs::Kind::Real:
        snprintf(buf, bufLen, "%.17g", sub.as<double>());
        break;
    case pvxs::Kind::Integer:
        snprintf(buf, bufLen, "%ld", sub.as<int64_t>());
        break;
    case pvxs::Kind::String:
        snprintf(buf, bufLen, "\"%s\"", sub.as<std::string>().c_str());
        break;
    default:
        break;
    }

    return true;
}

const char *pvsave::ntTypeString(const pvxs::Value &value) {
    auto sub = value["value"];
    if (sub)
        return sub.type().name();
    return nullptr;
}

void pvsave::ntToString(const pvxs::Value &value, char *buf, size_t bufLen) {}

static constexpr struct {
    const char *tn;
    pvxs::TypeCode::code_t code;
} TYPE_NAME_MAPPING[] = {
    {"int8_t", pvxs::TypeCode::Int8 },
    {"uint8_t", pvxs::TypeCode::UInt8 },
    {"int16_t", pvxs::TypeCode::Int16 },
    {"uint16_t", pvxs::TypeCode::UInt16 },
    {"int32_t", pvxs::TypeCode::Int32 },
    {"uint32_t", pvxs::TypeCode::UInt32 },
    {"int64_t", pvxs::TypeCode::Int64 },
    {"uint64_t", pvxs::TypeCode::UInt64 },
    {"float", pvxs::TypeCode::Float32 },
    {"double", pvxs::TypeCode::Float64 },
    {"string", pvxs::TypeCode::String },
};

std::pair<bool, pvxs::TypeCode> pvsave::ntTypeFromString(const char *ptype) {
    for (auto& p : TYPE_NAME_MAPPING) {
        if (!strcmp(ptype, p.tn))
            return {true, pvxs::TypeCode(p.code)};
    }
    return {false, pvxs::TypeCode()};
}

template<typename T> int parseValue(const char* str, T* out);
template<> int parseValue<epicsInt8>(const char* str, epicsInt8* out) { return epicsParseInt8(str, out, 10, nullptr); }
template<> int parseValue<epicsInt16>(const char* str, epicsInt16* out) { return epicsParseInt16(str, out, 10, nullptr); }
template<> int parseValue<epicsInt32>(const char* str, epicsInt32* out) { return epicsParseInt32(str, out, 10, nullptr); }
template<> int parseValue<epicsInt64>(const char* str, epicsInt64* out) { return epicsParseInt64(str, out, 10, nullptr); }
template<> int parseValue<epicsUInt8>(const char* str, epicsUInt8* out) { return epicsParseUInt8(str, out, 10, nullptr); }
template<> int parseValue<epicsUInt16>(const char* str, epicsUInt16* out) { return epicsParseUInt16(str, out, 10, nullptr); }
template<> int parseValue<epicsUInt32>(const char* str, epicsUInt32* out) { return epicsParseUInt32(str, out, 10, nullptr); }
template<> int parseValue<epicsUInt64>(const char* str, epicsUInt64* out) { return epicsParseUInt64(str, out, 10, nullptr); }
template<> int parseValue<epicsFloat32>(const char* str, epicsFloat32* out) { return epicsParseFloat32(str, out, nullptr); }
template<> int parseValue<epicsFloat64>(const char* str, epicsFloat64* out) { return epicsParseFloat64(str, out, nullptr); }

// FIXME: these overloads suck and will not work on some platforms.
template<> int parseValue<uint64_t>(const char* str, uint64_t* out) {
    *out = strtoul(str, nullptr, 10);
    return errno;
}

template<> int parseValue<int64_t>(const char* str, int64_t* out) {
    *out = strtoul(str, nullptr, 10);
    return errno;
}


template<typename T>
std::pair<bool, pvxs::Value> parseNtScalarFromNumericString(const char* pval, pvxs::TypeCode type) {
    static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value, "Must be integral or floating-point");

    T val;
    if (parseValue(pval, &val) != 0) {
        return {false, pvxs::Value{}};
    }

    pvxs::nt::NTScalar scalar(type);
    auto value = scalar.create();
    value["value"].from(val);
    return {true, value};
}

std::pair<bool, pvxs::Value> parseNtScalarFromString(const char* pval, pvxs::TypeCode type) {
    pvxs::nt::NTScalar scalar(type);
    auto value = scalar.create();
    value.from(std::string(pval));
    return {true, value};
}

std::pair<bool, pvxs::Value> pvsave::ntScalarFromString(const char *pval, pvxs::TypeCode type) {
    switch(type.code) {
    case pvxs::TypeCode::Int8:
        return parseNtScalarFromNumericString<epicsInt8>(pval, type);
    case pvxs::TypeCode::Int16:
        return parseNtScalarFromNumericString<epicsInt16>(pval, type);
    case pvxs::TypeCode::Int32:
        return parseNtScalarFromNumericString<epicsInt32>(pval, type);
    case pvxs::TypeCode::Int64:
        return parseNtScalarFromNumericString<epicsInt64>(pval, type);
    case pvxs::TypeCode::UInt8:
        return parseNtScalarFromNumericString<epicsUInt8>(pval, type);
    case pvxs::TypeCode::UInt16:
        return parseNtScalarFromNumericString<epicsUInt16>(pval, type);
    case pvxs::TypeCode::UInt32:
        return parseNtScalarFromNumericString<epicsUInt32>(pval, type);
    case pvxs::TypeCode::UInt64:
        return parseNtScalarFromNumericString<epicsUInt64>(pval, type);
    case pvxs::TypeCode::Float32:
        return parseNtScalarFromNumericString<float>(pval, type);
    case pvxs::TypeCode::Float64:
        return parseNtScalarFromNumericString<double>(pval, type);
    case pvxs::TypeCode::String:
        return parseNtScalarFromString(pval, type);
    default:
        return {false, pvxs::Value{}};
    }
}

int translateEscape(int c) {
    switch(c) {
    case 'a': return '\a';
    case 'b': return '\b';
    case 'e': return '\e';
    case 'f': return '\f';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case 'v': return '\v';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    case '?': return '?';
    case ' ': return ' ';
    default:
        return 0;
    }
}

const char* pvsave::parseString(const char* pstr, std::string& out) {
    bool q = false;
    const char* s = pstr;
    if (*s == '\"') {
        s++;
        q = true;
    }

    out.reserve(strlen(pstr));
    for (; *s; s++) {
        if (*s == '\\') {
            ++s;
            if (isspace(*s)) {
                out.push_back(*s);
                continue;
            }
            int e = translateEscape(*s);
            if (e) {
                out.push_back(e);
                continue;
            }
            return pstr;
        }
        else if (*s == '"') {
            ++s;
            return s;
        }
        else {
            out.push_back(*s);
        }
    }

    return q ? pstr : s;
}

void pvsave::pindent(FILE* fp, int indent) {
    for (int i = 0; i < indent; ++i)
        fputc(' ', fp);
}

static constexpr struct { const char* name; dbfType type; } STR_TO_DBTYPE[] =
{
    { "string", DBF_STRING },
    { "int8_t", DBF_CHAR },
    { "uint8_t", DBF_UCHAR },
    { "int16_t", DBF_SHORT },
    { "uint16_t", DBF_USHORT },
    { "int32_t", DBF_LONG },
    { "uint32_t", DBF_ULONG },
    { "int64_t", DBF_INT64 },
    { "uint64_t", DBF_UINT64 },
    { "float32", DBF_FLOAT },
    { "float64", DBF_DOUBLE },
    { "enum", DBF_ENUM },
    { "menu", DBF_MENU },
    { "device", DBF_DEVICE },
    { "inlnk", DBF_INLINK },
    { "outlnk", DBF_OUTLINK },
    { "fwdlnk", DBF_FWDLINK },
    { "noaccess", DBF_NOACCESS }
};

static constexpr const char* DBTYPE_TO_STR[] =
{
    "string",       /*DBF_STRING*/
    "int8_t",       /*DBF_CHAR*/
    "uint8_t",      /*DBF_UCHAR*/
    "int16_t",      /*DBF_SHORT*/
    "uint16_t",     /*DBF_USHORT*/
    "int32_t",      /*DBF_LONG*/
    "uint32_t",     /*DBF_ULONG*/
    "int64_t",      /*DBF_INT64*/
    "uint64_t",     /*DBF_UINT64*/
    "float32",      /*DBF_FLOAT*/
    "float64",      /*DBF_DOUBLE*/
    "enum",         /*DBF_ENUM*/
    "menu",         /*DBF_MENU*/
    "device",       /*DBF_DEVICE*/
    "inlnk",        /*DBF_INLINK*/
    "outlnk",       /*DBF_OUTLINK*/
    "fwdlnk",       /*DBF_FWDLINK*/
    "noaccess",     /*DBF_NOACCES*/
};

const char* pvsave::dbTypeString(dbfType ftype) {
    assert(ftype >= 0 && ftype < DBF_NTYPES);
    return DBTYPE_TO_STR[ftype];
}

dbfType pvsave::dbTypeFromString(const char* str) {
    for (auto& p : STR_TO_DBTYPE) {
        if (!strcmp(str, p.name))
            return p.type;
    }
    return DBF_NOACCESS;
}

static constexpr const char* TYPECODE_TO_STR[] =
{
	"void",     /*VOID = 0,*/
	"int8_t",   /*INT8,*/
    "uint8_t",  /*UINT8,*/
    "int16_t",  /*INT16,*/
    "uint16_t", /*UINT16,*/
	"int32_t",  /*INT32,*/
    "uint32_t", /*UINT32,*/
    "int64_t",  /*INT64,*/
    "uint64_t", /*UINT64,*/
	"string",   /*STRING,*/
    "float32",  /*FLOAT,*/
    "float64",  /*DOUBLE,*/
    "", /*POINTER,*/
    "", /*REFERENCE,*/
    "", /*CHAR,*/
    "", /*CSTRING,*/
	"", /*OTHER,*/
};

const char* pvsave::typeCodeString(ETypeCode code) {
	return TYPECODE_TO_STR[(int)code];
}

static constexpr struct { const char* ps; pvsave::ETypeCode c; } STR_TO_TYPECODE[] =
{
	{ "void",     pvsave::ETypeCode::VOID },
	{ "int8_t",   pvsave::ETypeCode::INT8 },
    { "uint8_t",  pvsave::ETypeCode::UINT8 },
    { "int16_t",  pvsave::ETypeCode::INT16 },
    { "uint16_t", pvsave::ETypeCode::UINT16 },
	{ "int32_t",  pvsave::ETypeCode::INT32 },
    { "uint32_t", pvsave::ETypeCode::UINT32 },
    { "int64_t",  pvsave::ETypeCode::INT64 },
    { "uint64_t", pvsave::ETypeCode::UINT64 },
	{ "string",   pvsave::ETypeCode::STRING },
    { "float32",  pvsave::ETypeCode::FLOAT },
    { "float64",  pvsave::ETypeCode::DOUBLE },
    //{ "", /*POINTER,*/ },
    //{ "", /*REFERENCE,*/ },
    //{ "", /*CHAR,*/ },
    //{ "", /*CSTRING,*/ },
	//{ "", /*OTHER,*/ },
};

std::pair<bool, pvsave::ETypeCode> pvsave::typeCodeFromString(const char* str) {
    for (auto& p : STR_TO_TYPECODE) {
        if (!strcmp(p.ps, str))
            return {true, p.c};
    }
    return {false, ETypeCode::VOID};
}

template<typename T>
void parseNumericDataFromString(pvsave::Data& out, const char* string) {
    out.construct<T>();
    if (parseValue(string, out.get<T>()) != 0) {
        out.clear();
    }
}

std::pair<bool, pvsave::Data> pvsave::dataParseString(const char* pstring, ETypeCode expected) {
    Data d;
    switch(expected) {
    case ETypeCode::INT8:
        parseNumericDataFromString<int8_t>(d, pstring); break;
    case ETypeCode::UINT8:
        parseNumericDataFromString<uint8_t>(d, pstring); break;
    case ETypeCode::INT16:
        parseNumericDataFromString<int16_t>(d, pstring); break;
    case ETypeCode::UINT16:
        parseNumericDataFromString<uint16_t>(d, pstring); break;
    case ETypeCode::INT32:
        parseNumericDataFromString<int32_t>(d, pstring); break;
    case ETypeCode::UINT32:
        parseNumericDataFromString<uint32_t>(d, pstring); break;
    case ETypeCode::INT64:
        parseNumericDataFromString<int64_t>(d, pstring); break;
    case ETypeCode::UINT64:
        parseNumericDataFromString<uint64_t>(d, pstring); break;
    case ETypeCode::FLOAT:
        parseNumericDataFromString<float>(d, pstring); break;
    case ETypeCode::DOUBLE:
        parseNumericDataFromString<double>(d, pstring); break;
    case ETypeCode::STRING:
        d.construct<std::string>(pstring); break;
    default: break;
    }
    return {d.type_code() != ETypeCode::VOID, d};
}

bool pvsave::dataToString(const Data& data, char* outBuf, size_t bufLen) {
    switch(data.type_code()) {
    case ETypeCode::INT8:
        snprintf(outBuf, bufLen, "%d", data.value<int8_t>()); break;
    case ETypeCode::UINT8:
        snprintf(outBuf, bufLen, "%d", data.value<uint8_t>()); break;
    case ETypeCode::INT16:
        snprintf(outBuf, bufLen, "%d", data.value<int16_t>()); break;
    case ETypeCode::UINT16:
        snprintf(outBuf, bufLen, "%d", data.value<uint16_t>()); break;
    case ETypeCode::INT32:
        snprintf(outBuf, bufLen, "%d", data.value<int32_t>()); break;
    case ETypeCode::UINT32:
        snprintf(outBuf, bufLen, "%u", data.value<uint32_t>()); break;
    case ETypeCode::INT64:
        snprintf(outBuf, bufLen, "%ld", data.value<int64_t>()); break;
    case ETypeCode::UINT64:
        snprintf(outBuf, bufLen, "%lu", data.value<uint64_t>()); break;
    case ETypeCode::FLOAT:
        snprintf(outBuf, bufLen, "%.17g", data.value<float>()); break;
    case ETypeCode::DOUBLE:
        snprintf(outBuf, bufLen, "%.17g", data.value<double>()); break;
    case ETypeCode::STRING:
        strncpy(outBuf, data.value<std::string>().c_str(), bufLen);
        outBuf[bufLen-1] = 0;
        break;
    default:
        return false;
    }
    return true;
}