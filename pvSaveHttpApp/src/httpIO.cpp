
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "epicsAssert.h"
#include "epicsExport.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "iocsh.h"

#include "pvsave/pvSave.h"
#include "pvsave/serialize.h"

#include "curl/curl.h"

constexpr int MAX_LINE_LENGTH = 4096;

namespace pvsave {

class httpIO : public pvsave::pvSaveIO {
public:
    httpIO(const char *name, const char *url)
        : pvsave::pvSaveIO(name), url_(url) {}

    uint32_t flags() const override { return Write | Read; /* Both read and write supported */ }

    bool initCurl() {
        if (!curl_) {
            curl_ = curl_easy_init();
            curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
        }
        return !!curl_;
    }

    bool beginWrite() override { return initCurl(); }

    bool writeData(const std::vector<DataSource::Channel> &pvNames, const std::vector<Data>& pvValues, size_t pvCount) override {
        const char *funcName = "HTTPIO::writeData";

        //std::string urlReq = url_ + "/pvput?";
        std::string urlReq;
        std::string reqUrl = url_ + "/pvput";
        for (size_t i = 0; i < pvCount; ++i) {

            char line[MAX_LINE_LENGTH];
            line[0] = 0;

            strncat(line, pvNames[i].channelName.c_str(), sizeof(line)-1);
            strncat(line, " ", sizeof(line)-1);
            strncat(line, pvsave::typeCodeString(pvValues[i].type_code()), sizeof(line)-1);
            strncat(line, " ", sizeof(line)-1);

            char buf[MAX_LINE_LENGTH];
            if (!dataToString(pvValues[i], buf, sizeof(buf))) {
                printf("Unable to serialize %s\n", pvNames[i].channelName.c_str());
                continue;
            }
            strncat(line, buf, sizeof(line)-1);
            strncat(line, "\n", sizeof(line)-1);

            if (i > 0 && i < pvCount)
                urlReq.append("&");
            auto* ps = curl_easy_escape(curl_, line, strlen(line));
            urlReq.append(ps);
            free(ps);
            //urlReq.append(line);
        }

        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, urlReq.c_str());

        printf("PUT: %s\n", urlReq.c_str());
        curl_easy_setopt(curl_, CURLOPT_URL, reqUrl.c_str());

        int result = curl_easy_perform(curl_);

        if (result != CURLE_OK) {
            printf("Failed to HTTP the data: %d\n", result);
            return false;
        }

        return true;
    }

    bool endWrite() override {
        return true;
    }

    bool beginRead() override { return initCurl(); }

#if 0
    bool readText(std::vector<std::string> &pvNames, std::vector<Data> &pvValues) {
        
        const char *funcName = "HTTPIO::readText";

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

            auto typeCode = pvsave::typeCodeFromString(ptype);
            if (!typeCode.first) {
                printf("%s: file %s, line %d: unknown type name '%s'\n", funcName, path_.c_str(), line, ptype);
                continue;
            }

            std::string parsedValue;
            if (pvsave::parseString(pval, parsedValue) == pval) {
                printf("%s: file %s, line %d: failed to parse value string\n", funcName, path_.c_str(), line);
                continue;
            }

            auto value = pvsave::dataParseString(parsedValue.c_str(), typeCode.second);
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
#endif

    bool readData(std::unordered_map<std::string, Data>& pvs) override {
        const char *funcName = "HTTPIO::readData";

        return false;
    }

    bool endRead() override { return true; }

    void report(FILE* fp, int indent) override {
        pvsave::pindent(fp, indent);
        fprintf(fp, "HTTPIO\n");
        pvsave::pindent(fp, indent);
        fprintf(fp, "url: %s", url_.c_str());
        pvsave::pindent(fp, indent);
        fprintf(fp, "flags: %s%s\n", (flags() & Read) ? "r" : "", (flags() & Write) ? "w" : "");
    }

protected:
    std::string url_;
    CURL* curl_ = nullptr;
};

} // namespace pvsave

static void pvsConfigureHTTPIOCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvsConfigureHTTPIO";
    const char *ioName = buf[0].sval;
    const char *url = buf[1].sval;

    if (!url || !ioName) {
        printf("%s: url and ioName must be provided", funcName);
        return;
    }


    new pvsave::httpIO(ioName, url);
}

void registerHTTPIO() {
    /* pvsConfigureHTTPIO */
    {
        static iocshArg arg0 = {"ioName", iocshArgString};
        static iocshArg arg1 = {"url", iocshArgString};
        static iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvsConfigureHTTPIO", 2, args};
        iocshRegister(&funcDef, pvsConfigureHTTPIOCallFunc);
    }
}

epicsExportRegistrar(registerHTTPIO);