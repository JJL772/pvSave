
#include <stdlib.h>
#include <string.h>
#include <unordered_map>

#include "epicsStdlib.h"

#include "serialize.h"

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

template<typename T>
std::pair<bool, pvxs::Value> parseNtScalarFromNumericString(const char* pval, pvxs::TypeCode type) {
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "Must be integral or floating-point");

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