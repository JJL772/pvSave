
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "epicsAssert.h"
#include "epicsExport.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "iocsh.h"

#include "pvxs/data.h"
#include "pvxs/nt.h"

#include "pvSaveIO.h"
#include "serialize.h"

constexpr int MAX_LINE_LENGTH = 4096;

enum fileSystemIOType { FSIO_TYPE_TEXT, FSIO_TYPE_JSON };

class fileSystemIO : public pvsave::pvSaveIO {
public:
    fileSystemIO(const char *name, const char *filePath, fileSystemIOType type)
        : pvsave::pvSaveIO(name), type_(type), path_(filePath) {}

    uint32_t flags() const override { return Write | Read; /* Both read and write supported */ }

    bool openFile() {
        if (!handle_)
            handle_ = fopen(path_.c_str(), "r+b"); /** Don't truncate file on load, but open in RDWR mode */

        if (!handle_) {
            printf("fileSystemIO::beginWrite: Failed to open '%s': %s\n", path_.c_str(), strerror(errno));
            return false;
        }
        return true;
    }

    bool beginWrite() override { return openFile(); }

    bool saveText(const std::string *pvNames, const pvxs::Value *pvValues, size_t pvCount) {
        const char *funcName = "fileSystemIO::saveText";
        for (int i = 0; i < pvCount; ++i) {
            /** PV name */
            if (fputs(pvNames[i].c_str(), handle_) < 0) {
                printf("%s: fputs failed: %s\n", funcName, strerror(errno));
                return false;
            }
            fputc(' ', handle_);

            const auto &value = pvValues[i];

            /** Type string */
            fputs(pvsave::ntTypeString(value), handle_);
            fputc(' ', handle_);

            /** Value */
            if (!strncmp(value.id().c_str(), "epics:nt/NTScalar", sizeof("epics:nt/NTScalar") - 1)) {
                char line[MAX_LINE_LENGTH];
                pvsave::ntScalarToString(value, line, sizeof(line));
                fputs(line, handle_);
            } else {
                printf("Unsupported type %s\n", value.id().c_str());
            }

            fputc('\n', handle_);
        }
        return true;
    }

    bool writeData(const std::string *pvNames, const pvxs::Value *pvValues, size_t pvCount) override {
        const char *funcName = "fileSystemIO::writeData";
        if (fseek(handle_, 0, SEEK_SET) != 0) {
            printf("%s: fseek failed: %s\n", funcName, strerror(errno));
            return false;
        }

        if (ftruncate(fileno(handle_), 0) < 0) {
            printf("%s: ftruncate failed: %s\n", funcName, strerror(errno));
            return false;
        }

        switch (type_) {
        case FSIO_TYPE_TEXT:
            return saveText(pvNames, pvValues, pvCount);
        case FSIO_TYPE_JSON: /** TODO: implement me! */
        default:
            break;
        }

        return false;
    }

    bool endWrite() override {
        fflush(handle_);
        return true;
    }

    bool beginRead() override { return openFile(); }

    bool readText(std::vector<std::string> &pvNames, std::vector<pvxs::Value> &pvValues) {
        const char *funcName = "fileSystemIO::readText";

        char *lptr = nullptr;
        size_t len = 0;

        int result, line;
        for (result = getline(&lptr, &len, handle_), line = 1; result > 0;
             free(lptr), lptr = nullptr, result = getline(&lptr, &len, handle_), ++line) {

            char *sp = nullptr, *pname = nullptr, *ptype = nullptr, *pval = nullptr;

            /** PV name */
            pname = strtok_r(lptr, " ", &sp);
            if (!pname) {
                printf("%s: file %s, line %d: missing PV name\n", funcName, path_.c_str(), line);
                continue;
            }

            /** PV type */
            ptype = strtok_r(nullptr, " ", &sp);
            if (!ptype) {
                printf("%s: file %s, line %d: missing PV type\n", funcName, path_.c_str(), line);
                continue;
            }

            /** PV value */
            pval = strtok_r(nullptr, " ", &sp);
            if (!pval) {
                printf("%s: file %s, line %d: missing PV value\n", funcName, path_.c_str(), line);
                continue;
            }

            auto ntType = pvsave::ntTypeFromString(ptype);
            if (!ntType.first) {
                printf("%s: file %s, line %d: unknown type name '%s'\n", funcName, path_.c_str(), line, ptype);
                continue;
            }

            std::string parsedValue;
            if (pvsave::parseString(pval, parsedValue) == pval) {
                printf("%s: file %s, line %d: failed to parse value string\n", funcName, path_.c_str(), line);
                continue;
            }

            auto value = pvsave::ntScalarFromString(parsedValue.c_str(), ntType.second);
            if (!value.first) {
                printf("%s: file %s, line %d: unable to parse value '%s'\n", funcName, path_.c_str(), line, pval);
                continue;
            }

            pvValues.push_back(value.second);
            pvNames.push_back(pname);
        }

        if (result == -1 && errno != 0) {
            printf("%s: getline failed: %s\n", funcName, strerror(errno));
            return false;
        }
        return true;
    }

    bool readData(std::vector<std::string> &pvNames, std::vector<pvxs::Value> &pvValues) override {
        const char *funcName = "fileSystemIO::readData";
        if (fseek(handle_, 0, SEEK_SET) != 0) {
            printf("%s: fseek failed: %s\n", funcName, strerror(errno));
        }

        switch (type_) {
        case FSIO_TYPE_TEXT:
            return readText(pvNames, pvValues);
        case FSIO_TYPE_JSON: /** TODO: implement me! */
        default:
            break;
        }

        return false;
    }

    bool endRead() override { return true; }

protected:
    fileSystemIOType type_;
    std::string path_;
    FILE *handle_;
};

static void pvsConfigureFileSystemIOCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvsConfigureFileSystemIO";
    const char *ioName = buf[0].sval;
    const char *filePath = buf[1].sval;
    const char *fileFormat = buf[2].sval;

    if (!filePath || !ioName) {
        printf("%s: filePath and ioName must be provided", funcName);
        return;
    }

    fileFormat = fileFormat ? fileFormat : "text";

    fileSystemIOType type = FSIO_TYPE_TEXT;
    if (!epicsStrCaseCmp(fileFormat, "text"))
        type = FSIO_TYPE_TEXT;
    else if (!epicsStrCaseCmp(fileFormat, "json"))
        type = FSIO_TYPE_JSON;

    new fileSystemIO(ioName, filePath, type);
}

void registerFSIO() {
    /* pvsConfigureFileSystemIO */
    {
        static iocshArg arg0 = {"ioName", iocshArgString};
        static iocshArg arg1 = {"filePath", iocshArgString};
        static iocshArg arg2 = {"fileFormat", iocshArgString};
        static iocshArg *args[] = {&arg0, &arg1, &arg2};
        static iocshFuncDef funcDef = {"pvsConfigureFileSystemIO", 3, args};
        iocshRegister(&funcDef, pvsConfigureFileSystemIOCallFunc);
    }
}

epicsExportRegistrar(registerFSIO);