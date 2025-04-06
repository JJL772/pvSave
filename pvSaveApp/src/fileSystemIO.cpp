/**
 * ----------------------------------------------------------------------------
 * Company    : SLAC National Accelerator Laboratory
 * ----------------------------------------------------------------------------
 * Description: I/O backend for saving PV data to a file on disk.
 * ----------------------------------------------------------------------------
 * This file is part of 'pvSave'. It is subject to the license terms in the
 * LICENSE.txt file found in the top-level directory of this distribution,
 * and at:
 *    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 * No part of 'pvSave', including this file, may be copied, modified,
 * propagated, or distributed except according to the terms contained in the
 * LICENSE.txt file.
 * ----------------------------------------------------------------------------
 **/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "epicsAssert.h"
#include "epicsExport.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "iocsh.h"
#include "yajl_parse.h"
#include "errlog.h"

#include "pvsave/pvSave.h"
#include "pvsave/serialize.h"

#include "common.h"

constexpr int MAX_LINE_LENGTH = 4096;

namespace pvsave {

enum fileSystemIOType { FSIO_TYPE_TEXT, FSIO_TYPE_JSON };

/**
 * \brief Implementation of the file system IO backend
 * Supports an autosave-like text format and JSON parsed using yajl
 */
class fileSystemIO : public pvsave::SaveRestoreIO {
public:
    fileSystemIO(const char *name, const char *filePath, fileSystemIOType type)
        : pvsave::SaveRestoreIO(name), type_(type), path_(filePath) {}

    uint32_t flags() const override { return Write | Read; /* Both read and write supported */ }

    bool openFile();

    bool beginWrite() override;
    bool beginWriteJson();
    bool saveText(const DataSource::Channel &channel, const Data &data);
    bool saveJson(const DataSource::Channel &channel, const Data &data);
    bool writeData(const DataSource::Channel &channel, const Data& pvValue) override;
    void endWriteJson();
    bool endWrite() override;

    /** Reading interface */
    bool beginRead() override { return openFile(); }
    bool readText(std::unordered_map<std::string, Data>& pvs);
    bool readJson(std::unordered_map<std::string, Data>& pvs);
    bool readData(std::unordered_map<std::string, Data>& pvs) override;
    bool endRead() override { return true; }

    void report(FILE* fp, int indent) override;

protected:
    fileSystemIOType type_;
    std::string path_;
    size_t currentChan_;        // Hack for JSON trailing commas
    FILE *handle_ = nullptr;
};

bool fileSystemIO::beginWrite() {
    constexpr const char* funcName = "fileSystemIO::beginWrite";
    if (!openFile())
        return false;

    // Start from a clean slate; seek to start and truncate
    if (fseek(handle_, 0, SEEK_SET) != 0) {
        LOG_ERR("%s: fseek failed: %s\n", funcName, strerror(errno));
        return false;
    }
    
    if (ftruncate(fileno(handle_), 0) < 0) {
        LOG_ERR("%s: ftruncate failed: %s\n", funcName, strerror(errno));
        return false;
    }
        
    if (type_ == FSIO_TYPE_JSON)
        return beginWriteJson();
    return true;
}

bool fileSystemIO::endWrite() {
    if (type_ == FSIO_TYPE_JSON)
        endWriteJson();
    fflush(handle_);
    return true;
}

bool fileSystemIO::openFile() {
    currentChan_ = 0;

    if (!handle_)
        handle_ = fopen(path_.c_str(), "a+b"); // Don't truncate file on load, but open in RDWR mode

    if (!handle_) {
        printf("fileSystemIO::beginWrite: Failed to open '%s': %s\n", path_.c_str(), strerror(errno));
        return false;
    }
    return true;
}

/**
 * Read data off disk
 */
bool fileSystemIO::readData(std::unordered_map<std::string, Data>& pvs) {
    const char *funcName = "fileSystemIO::readData";
    if (fseek(handle_, 0, SEEK_SET) != 0) {
        printf("%s: fseek failed: %s\n", funcName, strerror(errno));
    }

    switch (type_) {
    case FSIO_TYPE_TEXT:
        return readText(pvs);
    case FSIO_TYPE_JSON: /** TODO: implement me! */
        return readJson(pvs);
    default:
        break;
    }

    return false;
}

/**
 * \brief Save implementation of autosave-like .SAV files
 */
bool fileSystemIO::saveText(const DataSource::Channel& channel, const Data &value) {
    const char *funcName = "fileSystemIO::saveText";

    // PV name
    if (fputs(channel.channelName.c_str(), handle_) < 0) {
        LOG_ERR("%s: fputs failed: %s\n", funcName, strerror(errno));
        return false;
    }
    fputc(' ', handle_);

    // Type string
    fputs(pvsave::typeCodeString(value.type_code()), handle_);
    fputc(' ', handle_);

    // Value
    char line[MAX_LINE_LENGTH];
    line[0] = 0;
    if (!dataToString(value, line, sizeof(line))) {
        LOG_ERR("Unable to serialize %s\n", channel.channelName.c_str());
    }

    fputs(line, handle_);
    fputc('\n', handle_);
    return true;
}

/**
 * \brief Save implementation for JSON
 */
bool fileSystemIO::saveJson(const DataSource::Channel &channel, const Data &value) {
    const char *funcName = "fileSystemIO::saveJson";
    
    // Hack for json trailing commas. Need to finish off previous line, if any
    if (currentChan_ > 0)
        fputs(",\n", handle_);
    else
        fputs("\n", handle_);
    pindent(handle_, 1);

    // PV name and type
    if (fprintf(handle_, "\"%s#%s\": ", channel.channelName.c_str(), pvsave::typeCodeString(value.type_code())) < 0) {
        LOG_ERR("%s: fputs failed: %s\n", funcName, strerror(errno));
        return false;
    }

    // Value
    char line[MAX_LINE_LENGTH];
    line[0] = 0;
    if (!dataToString(value, line, sizeof(line))) {
        LOG_ERR("Unable to serialize %s\n", channel.channelName.c_str());
    }

    if (fprintf(handle_, "\"%s\"", line) < 0) {
        LOG_ERR("%s: fprintf failed: %s\n", funcName, strerror(errno));
    }

    fflush(handle_);
    
    currentChan_++;
    return true;
}

bool fileSystemIO::beginWriteJson() {
    return fputs("{\n", handle_) >= 0;
}

void fileSystemIO::endWriteJson() {
    fputs("\n}\n", handle_);
}

/**
 * Write data to disk
 */
bool fileSystemIO::writeData(const DataSource::Channel &channel, const Data &value) {
    switch (type_) {
    case FSIO_TYPE_TEXT:
        return saveText(channel, value);
    case FSIO_TYPE_JSON:
        return saveJson(channel, value);
    default:
        break;
    }
    return false;
}

/**
 * \brief Implementation of JSON reading using yajl
 */
bool fileSystemIO::readJson(std::unordered_map<std::string, Data>& pvs) {
    static const char* funcName = "fileSystemIO::readJson";
    bool success = true;

    yajl_alloc_funcs af = {
        .malloc = [](void* c, size_t s) { return malloc(s); },
        .realloc = [](void* c, void* p, size_t s) { return realloc(p, s); },
        .free = [](void* c, void* p) { free(p); }
    };

    struct JsonReadState {
        std::unordered_map<std::string, Data>& pvs;
        ETypeCode type;
        std::string curPv;
        bool skip;
    } jsonReadState {pvs};

    yajl_callbacks cb = {
        .yajl_string = [](void* c, const unsigned char* value, size_t l) -> int {
            auto pc = static_cast<JsonReadState*>(c);
            if (!pc->skip) {
                auto* pstr = const_cast<unsigned char*>(value);
                // FIXME: SUCKS!! avoid copy, directly modify string...
                auto c = *(pstr + l);
                *(pstr + l) = 0;
                auto result = dataParseString((const char*)pstr, pc->type);
                *(pstr + l) = c;
                if (!result.first) {
                    LOG_ERR("%s: Unable to parse data for %s\n", funcName, pc->curPv.c_str());
                }
                else {
                    pc->pvs.insert({pc->curPv, result.second});
                }
            }
            return 1;
        },
        .yajl_map_key = [](void* c, const unsigned char* key, size_t l) -> int {
            auto pc = static_cast<JsonReadState*>(c);
            pc->curPv.assign((const char*)key, l);
            pc->skip = false;
            // Types are specified in the PV name, denoted by a # prefix. i.e. myCool:PV:Or:Something#uint32_t
            // this could probably be implemented better, but this is the cheapest way to do it
            auto sep = pc->curPv.find_last_of('#');
            if (sep == pc->curPv.npos) {
                LOG_ERR("%s: Missing typecode for PV '%s'\n", funcName, key);
                pc->skip = true; // Skip if errored
            }
            else {
                pc->curPv.erase(sep);
                auto tc = pvsave::typeCodeFromString(pc->curPv.c_str()+sep+1);
                if (!tc.first) {
                    LOG_ERR("%s: Unknown type code %s\n", funcName, pc->curPv.c_str()+sep+1);
                    pc->skip = true; // Skip if errored
                }
                else
                    pc->type = tc.second;
            }
            return 1;
        }
    };

    yajl_handle yh = yajl_alloc(&cb, &af, &jsonReadState);
    if (!yh) {
        LOG_ERR("%s: Unable to alloc yajl handle!\n", funcName);
        return false;
    }

    // Alloc a working buffer in the heap (keep stack usage low!)
    size_t bs = 16384, nread = 0;
    unsigned char* rb = (unsigned char*)malloc(bs);
    while ((nread = fread(rb, 1, bs, handle_)) > 0) {
        if (yajl_parse(yh, rb, nread) != yajl_status_ok) {
            auto* errstr = yajl_get_error(yh, 1, rb, nread);
            LOG_ERR("%s: yajl_parse returned error: %s\n", funcName, errstr);
            free(errstr);
            success = false;
            goto done;
        }
    }

    // Flush out any remaining bytes
    yajl_complete_parse(yh);
done:
    yajl_free(yh);
    free(rb);
    return success;
}

/**
 * Implementation of autosave-like text format for SAV files
 */
bool fileSystemIO::readText(std::unordered_map<std::string, Data> &pvs) {
    const char *funcName = "fileSystemIO::readText";

    const size_t bl = 16384;
    char *buf = (char *)malloc(bl);

    char *lptr = nullptr;
    size_t len = 0;

    int line;
    for (lptr = fgets(buf, bl, handle_), line = 1; lptr != nullptr;
         lptr = nullptr, lptr = fgets(buf, bl, handle_), ++line) {

        len = strlen(lptr); // len is not actually string length, it's buffer length

        // Strip off delimeter
        if (len > 0)
            lptr[--len] = 0;

        // Skip empty lines
        if (!len)
            continue;

        char *sp = nullptr, *pname = nullptr, *ptype = nullptr, *pval = nullptr;

        // PV name
        pname = strtok_r(lptr, " ", &sp);
        if (!pname) {
            LOG_ERR("%s: file %s, line %d: missing PV name\n", funcName, path_.c_str(), line);
            continue;
        }

        // PV type
        ptype = strtok_r(nullptr, " ", &sp);
        if (!ptype) {
            LOG_ERR("%s: file %s, line %d: missing PV type\n", funcName, path_.c_str(), line);
            continue;
        }

        // PV value
        pval = strtok_r(nullptr, " ", &sp);
        if (!pval) {
            LOG_ERR("%s: file %s, line %d: missing PV value\n", funcName, path_.c_str(), line);
            continue;
        }

        // Determine and validate type
        auto typeCode = pvsave::typeCodeFromString(ptype);
        if (!typeCode.first) {
            LOG_ERR("%s: file %s, line %d: unknown type name '%s'\n", funcName, path_.c_str(), line, ptype);
            continue;
        }

        std::string parsedValue;
        if (pvsave::parseString(pval, parsedValue) == pval) {
            LOG_ERR("%s: file %s, line %d: failed to parse value string\n", funcName, path_.c_str(), line);
            continue;
        }

        // Parse the data into a variant
        auto value = pvsave::dataParseString(parsedValue.c_str(), typeCode.second);
        if (!value.first) {
            LOG_ERR("%s: file %s, line %d: unable to parse value '%s'\n", funcName, path_.c_str(), line, pval);
            continue;
        }

        pvs.insert({pname, value.second});
    }

    if (!lptr && errno != 0 && errno != EOF) {
        LOG_ERR("%s: getline failed: %s\n", funcName, strerror(errno));
        return false;
    }
    return true;
}

/**
 * Print out handy information about this I/O backend
 */
void fileSystemIO::report(FILE* fp, int indent) {
    pvsave::pindent(fp, indent);
    fprintf(fp, "fileSystemIO\n");
    pvsave::pindent(fp, indent);
    fprintf(fp, "type: ");
    switch(type_) {
    case FSIO_TYPE_JSON:
        fprintf(fp, "json\n"); break;
    case FSIO_TYPE_TEXT:
        fprintf(fp, "text\n"); break;
    default:
        break;
    }
    pvsave::pindent(fp, indent);
    fprintf(fp, "flags: %s%s\n", (flags() & Read) ? "r" : "", (flags() & Write) ? "w" : "");
    pvsave::pindent(fp, indent);
    fprintf(fp, "file: %s\n", path_.c_str());
}

} // namespace pvsave

static void pvSave_ConfigureFileSystemIOCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvSave_ConfigureFileSystemIO";
    const char *ioName = buf[0].sval;
    const char *filePath = buf[1].sval;
    const char *fileFormat = buf[2].sval;

    if (!filePath || !ioName) {
        printf("%s: filePath and ioName must be provided", funcName);
        return;
    }

    fileFormat = fileFormat ? fileFormat : "text";

    pvsave::fileSystemIOType type = pvsave::FSIO_TYPE_TEXT;
    if (!epicsStrCaseCmp(fileFormat, "text"))
        type = pvsave::FSIO_TYPE_TEXT;
    else if (!epicsStrCaseCmp(fileFormat, "json"))
        type = pvsave::FSIO_TYPE_JSON;

    new pvsave::fileSystemIO(ioName, filePath, type);
}

void registerFSIO() {
    /* pvsConfigureFileSystemIO */
    {
        static iocshArg arg0 = {"ioName", iocshArgString};
        static iocshArg arg1 = {"filePath", iocshArgString};
        static iocshArg arg2 = {"fileFormat", iocshArgString};
        static iocshArg *args[] = {&arg0, &arg1, &arg2};
        static iocshFuncDef funcDef = {"pvSave_ConfigureFileSystemIO", 3, args};
        iocshRegister(&funcDef, pvSave_ConfigureFileSystemIOCallFunc);
    }
}

epicsExportRegistrar(registerFSIO);