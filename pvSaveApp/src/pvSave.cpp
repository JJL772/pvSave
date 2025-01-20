
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include <list>
#include <memory>
#include <stdio.h>

#include "pvsave/pvSave.h"
#include "common.h"

#include "epicsAlgorithm.h"
#include "epicsExport.h"
#include "epicsStdio.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "initHooks.h"
#include "iocsh.h"
#include "macLib.h"
#include "epicsTime.h"
#include "dbScan.h"
#include "macLib.h"

using namespace pvsave;

constexpr int MAX_LINE_LENGTH = 1024;

static int s_configuredThreadPriority = epicsThreadPriorityLow;
static epicsThreadId s_threadId = 0;

static epicsTimeStamp s_lastProcTime;
static int s_lastStatus;

/** Global list of IO backend instances */
std::unordered_map<std::string, pvsave::pvSaveIO *> &pvsave::ioBackends() {
    static std::unordered_map<std::string, pvsave::pvSaveIO *> b;
    return b;
}

pvsave::DataSource* pvsave::dataSource() {
    static DataSource* s;
    if (!s) {
        s = createDataSourceCA();
    }
    return s;
}

/**
 * Guard mutex for the context list
 */
epicsMutex& contextGuard() {
    static epicsMutex mutex;
    return mutex;
}

/**
 * \brief Describes a set of PVs to be monitored and saved at a specific rate
 * A monitor set may have multiple IO backends associated with it
 */
struct MonitorSet {
    MonitorSet(const std::string &_name, double _period)
        : name(_name),
          period(_period),
          stage(-1) {}
    std::string name;
    double period;

    int stage;
    std::vector<pvsave::pvSaveIO *> io;
    std::vector<std::string> pvList;
    class SaveContext *context;
};

std::unordered_map<std::string, std::shared_ptr<MonitorSet>> monitorSets;

/**
 * \brief pvSave Context
 * Contexts are actual instantiations of monitor sets and contain runtime specific data
 * such as last processed time, connected channels, etc.
 */
class SaveContext {
public:
    SaveContext(std::shared_ptr<MonitorSet> set)
        : monitorSet_(set) {
        monitorSet_->context = this;
    }

    /**
     * Called after device support init to connect/monitor all relevant PVs
     */
    void init() {
        pvsave::dataSource()->connect(monitorSet_->pvList, channels_);
    }

    bool save() {
        lastStatus_ = 0;

        std::vector<pvsave::Data> data;
        data.resize(channels_.size());
        for (size_t i = 0; i < channels_.size(); ++i) {
            pvsave::dataSource()->get(channels_[i], data[i]);
        }

        /* Forward the data to each of the IO handlers */
        for (auto &io : monitorSet_->io) {
            if (io->flags() & pvsave::pvSaveIO::Write) {
                if (!io->beginWrite()) {
                    printf("pvSave: io->beginWrite: save failed\n");
                    lastStatus_ = 1;
                    continue;
                }

                if (!io->writeData(channels_, data, data.size())) {
                    printf("pvSave: io->writeData: save failed\n");
                    lastStatus_ = 1;
                    /* Fallthrough as we want the IO handler to do cleanup */
                }

                if (!io->endWrite()) {
                    lastStatus_ = 1;
                    printf("pvSave: io->endWrite: save failed\n");
                }
            }
        }

        return true;
    }

    bool restore(pvsave::pvSaveIO *io) {
        if (!(io->flags() & pvsave::pvSaveIO::Read))
            return false;

        printf("Restoring from %s\n", monitorSet_->name.c_str());

        if (!io->beginRead()) {
            printf("pvSave: io->beginRead: restore failed\n");
            return false;
        }

        std::unordered_map<std::string, pvsave::Data> pvs;
        if (!io->readData(pvs)) {
            printf("pvSave: io->readData: restore failed\n");
            /* Fallthrough to allow cleanup */
        }

        if (!io->endRead()) {
            printf("pvSave: io->endRead: restore failed\n");
        }

        for (size_t i = 0; i < channels_.size(); ++i) {
            auto it = pvs.find(channels_[i].channelName);
            if (it == pvs.end()) {
                continue; /* PV not found in save data */
            }
            pvsave::dataSource()->put(channels_[i], it->second);
        }

        return true;
    }

    bool restore() {
        bool restoredAny = false;
        for (auto &io : monitorSet_->io) {
            if (restore(io)) {
                restoredAny = true;
                break;
            }
        }

        if (!restoredAny) {
            printf("%s: restore failed: no backend was able to restore\n", "SaveContext::restore");
            return false;
        }
        return true;
    }

    /** Returns the channels */
    inline const std::vector<pvsave::DataSource::Channel> &channels() const { return channels_; }

    /** Returns the monitor set we were configured with */
    inline const std::shared_ptr<MonitorSet> &monitorSet() const { return monitorSet_; }

    static std::vector<SaveContext> saveContexts;

    epicsTimeStamp lastProc_ = {0,0};
    int lastStatus_ = 0;

protected:
    std::shared_ptr<MonitorSet> monitorSet_;
    std::vector<pvsave::DataSource::Channel> channels_;
    bool pendingRestore_ = false;
};

std::vector<SaveContext> SaveContext::saveContexts;

static bool readPvList(FILE* fp, const char* defs, std::vector<std::string>& list) {
    MAC_HANDLE* handle = nullptr;
    if (macCreateHandle(&handle, nullptr) != 0) {
        printf("readPvList: macCreateHandle failed\n");
        return false;
    }

    char** pairs = nullptr;
    if (macParseDefns(handle, defs, &pairs) < 0) {
        printf("readPvList: macParseDefns failed to parse definitions string\n");
        macDeleteHandle(handle);
        return false;
    }

    for (char** p = pairs; *p; p += 2)
        macPutValue(handle, *p, *(p+1));

    char* result = NULL;
    for(;;) {
        char buf[MAX_LINE_LENGTH];
        char* line = buf;

        result = fgets(buf, sizeof(buf), fp);
        if (result == NULL) break;

        /** Find start of comment and NULL terminate there */
        char* s = nullptr;
        if ((s = strpbrk(line, "#")))
            *s = 0;

        char expanded[MAX_LINE_LENGTH];
        long elen = 0;
        if ((elen = macExpandString(handle, line, expanded, sizeof(expanded))) < 0) {
            printf("readPvList: unexpanded macro string\n");
            /** Treat this as a success */
        }

        /** Skip leading whitespace */
        char* l = expanded;
        while (isspace(*l))
            l++;

        /** ..and trim off trailing */
        for (char* p = expanded+elen-1; *p && p >= expanded; p--) {
            if (isspace(*p))
                *p = 0;
        }

        /** Only insert if there's something left */
        if (*l) {
            list.push_back(l);
            printf("Adding '%s'\n", l);
        }

    }

    macDeleteHandle(handle);
    free(pairs);
    return true;
}

static bool readPvListFile(const char* file, const char* defs, std::vector<std::string>& list) {
    FILE*fp = fopen(file, "rb");
    if (!fp) {
        printf("readPvListFile: Unable to open %s\n", file);
        return false;
    }

    bool b = readPvList(fp, defs, list);

    fclose(fp);
    return b;
}

static std::shared_ptr<MonitorSet> findMonitorSet(const char *name) {
    auto it = monitorSets.find(name);
    if (it == monitorSets.end())
        return {};
    return it->second;
}

static void pvSave_CreatePvSetCallFunc(const iocshArgBuf *buf) {
    const char *name = buf[0].sval;
    double rate = buf[1].dval;

    if (rate < 10.0) {
        printf("pvSave_CreatePvSet: rate must be >= 10 (%f)\n", rate);
        iocshSetError(-1);
        return;
    }

    if (!name) {
        printf("pvSave_CreatePvSet expects 'name' parameter\n");
        iocshSetError(-1);
        return;
    }

    monitorSets.insert({name, std::make_shared<MonitorSet>(name, rate)});
}

static void pvSave_AddPvSetIOCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvSave_AddPvSetIO";
    const char *name = buf[0].sval;
    const char *ioName = buf[1].sval;

    if (!name || !ioName) {
        printf("%s: expected 'name' and 'ioName' parameter\n", funcName);
        iocshSetError(-1);
        return;
    }

    auto ms = findMonitorSet(name);
    if (!ms) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
        iocshSetError(-1);
        return;
    }

    auto it = pvsave::ioBackends().find(ioName);
    if (it != pvsave::ioBackends().end()) {
        ms->io.push_back(it->second);
    } else {
        printf("%s: No such IO backend '%s'\n", funcName, ioName);
        iocshSetError(-1);
        return;
    }
}

static void pvSave_AddPvSetPvCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvSave_AddPvSetPv";
    const char *name = buf[0].sval;
    const char *pvPattern = buf[1].sval;

    if (!name || !pvPattern) {
        printf("%s: expected 'name' and 'pvPattern' parameter\n", funcName);
        iocshSetError(-1);
        return;
    }

    auto mset = findMonitorSet(name);
    if (!mset) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
        iocshSetError(-1);
        return;
    }
    mset->pvList.push_back(pvPattern);
}

static void pvSave_AddPvSetListCallFunc(const iocshArgBuf* buf) {
    constexpr const char *funcName = "pvSave_AddPvSetList";
    const char *name = buf[0].sval;
    const char *file = buf[1].sval;
    const char *macros = buf[2].sval;

    if (!name || !file || !macros) {
        printf("%s: expected 'name', 'file' and 'macros' parameter\n", funcName);
        iocshSetError(-1);
        return;
    }

    auto mset = findMonitorSet(name);
    if (!mset) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
        iocshSetError(-1);
        return;
    }

    if (!readPvListFile(file, macros, mset->pvList)) {
        printf("%s: Unable to read '%s'\n", funcName, file);
        iocshSetError(-1);
    }
}

static void pvSave_SetPvSetRestoreStageCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvSave_SetPvSetRestoreStage";
    const char *name = buf[0].sval;
    const char *stage = buf[1].sval;

    if (!name || !stage) {
        printf("%s: expected 'name' and 'stage' parameter\n", funcName);
        iocshSetError(-1);
        return;
    }

    auto mset = findMonitorSet(name);
    if (!mset) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
        iocshSetError(-1);
        return;
    }

    /* Stage may be specified in an autosave-like way */
    epicsUInt32 st;
    if (epicsParseUInt32(stage, &st, 10, NULL) == 0) {
        switch (st) {
        case 0:
            mset->stage = initHookAfterInitDevSup;
            break;
        case 1:
            mset->stage = initHookAfterInitDatabase;
            break;
        case 2:
            mset->stage = initHookAfterIocRunning;
            break;
        default:
            printf("%s: invalid stage '%d': 0, 1 or 2 allowed\n", funcName, st);
            iocshSetError(-1);
            return;
        }
    }
    /* May also be a string */
    else {
        if (!epicsStrCaseCmp(stage, "AfterInitDevSup")) {
            mset->stage = initHookAfterInitDevSup;
        } else if (!epicsStrCaseCmp(stage, "AfterInitDatabase")) {
            mset->stage = initHookAfterInitDatabase;
        } else if (!epicsStrCaseCmp(stage, "AfterIocRunning")) {
            mset->stage = initHookAfterIocRunning;
        } else {
            printf("%s: invalid stage '%s': 'AfterInitDevSup' (0), 'AfterInitDatabase' (1), 'AfterIocRunning' (2) allowed\n", funcName, stage);
            iocshSetError(-1);
        }
    }
}

static void pvSave_ListPvSetsCallFunc(const iocshArgBuf *buf) {
    for (auto &pair : monitorSets) {
        printf("%s: %lu PVs\n", pair.first.c_str(), pair.second->pvList.size());
        printf("  IO ports:\n");
        for (int i = 0; i < pair.second->io.size(); ++i) {
            printf("   %d:\n", i);
            pair.second->io[i]->report(stdout, 5);
        }
    }
}

static void pvSave_ListChannelsCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvSave_ListChannels";
    const char *name = buf[0].sval;
    const char *filter = buf[1].sval;

    if (!name) {
        printf("%s: expected 'name' parameter\n", funcName);
        iocshSetError(-1);
        return;
    }

    auto mset = findMonitorSet(name);
    if (!mset) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
        iocshSetError(-1);
        return;
    }

#if 0
    bool onlyConnected = filter ? !epicsStrCaseCmp(filter, "connected") : false;
    bool onlyDisconnected = filter ? !epicsStrCaseCmp(filter, "disconnected") : false;

    if (mset->context) {
        printf("%s (%lu channels)\n", name, mset->context->channels().size());
        for (auto &conn : mset->context->channels()) {
            if (onlyConnected && !conn->connected())
                continue;
            if (onlyDisconnected && conn->connected())
                continue;
            printf("  %s: %s\n", conn->name().c_str(), conn->connected() ? "CONNECTED" : "DISCONNECTED");
        }
    } else {
        printf("No channels yet\n");
    }
#endif
}

static void pvSave_SetThreadPriorityCallFunc(const iocshArgBuf* buf) {
    constexpr const char* funcName = "pvSave_SetThreadPriority";
    const char* prio = buf[0].sval;

    if (s_threadId) {
        printf("%s: thread is already created; this function must be called before iocInit!\n", funcName);
        iocshSetError(-1);
        return;
    }

    if (!strcmp(prio, "low")) {
        s_configuredThreadPriority = epicsThreadPriorityLow;
    }
    else if (!strncmp(prio, "med", 3)) {
        s_configuredThreadPriority = epicsThreadPriorityMedium;
    }
    else if(!strcmp(prio, "high")) {
        s_configuredThreadPriority = epicsThreadPriorityHigh;
    }
    else if(!strcmp(prio, "max")) {
        s_configuredThreadPriority = epicsThreadPriorityMax;
    }
    else if(!strcmp(prio, "min")) {
        s_configuredThreadPriority = epicsThreadPriorityMin;
    }
    else {
        printf("%s: Unknown thread priority '%s': must be low, med, high, max or min\n", funcName, prio);
        iocshSetError(-1);
        return;
    }
}

epicsTimeStamp pvsave::lastProcTime(int ms) {
    if (ms < 0) {
        return s_lastProcTime;
    }
    else if (ms < SaveContext::saveContexts.size()) {
        return SaveContext::saveContexts[ms].lastProc_;
    }
    else {
        return {};
    }
}

int pvsave::lastStatus(int ms) {
    if (ms < 0) {
        return s_lastStatus;
    }
    else if (ms < SaveContext::saveContexts.size()) {
        return SaveContext::saveContexts[ms].lastStatus_;
    }
    else {
        return 0;
    }
}

void pvsave::saveAllNow() {
    epicsGuard<epicsMutex> guard(contextGuard());

    epicsTimeStamp now;
    epicsTimeGetCurrent(&now);

    s_lastStatus = 0;
    for (auto& context: SaveContext::saveContexts) {
        if (!context.save()) {
            s_lastStatus = 1;
            printf("Unable to save!\n");
        }
        context.lastProc_ = now;
    }

    /* Kick off I/O scan for status records */
    scanIoRequest(*statusIoScan());
}

static void pvSave_SaveCallFunc(const iocshArgBuf* buf) {
    printf("pvSave: Forcing save...\n");
    pvsave::saveAllNow();
}

static void pvSaveThreadProc(void *data) {
    epicsTimeStamp startTime;
    epicsTimeGetCurrent(&startTime);

    double sleepTime = 30.f;
    for (auto &context : SaveContext::saveContexts)
        sleepTime = epicsMin(context.monitorSet()->period, sleepTime);

    while (1) {
        epicsThreadSleep(sleepTime);
        sleepTime = 30.f;

        epicsTimeStamp now;
        epicsTimeGetCurrent(&now);

        epicsGuard<epicsMutex> guard(contextGuard());
        s_lastStatus = 0;
        for (auto &context : SaveContext::saveContexts) {
            double diff;
            if ((diff = epicsTimeDiffInSeconds(&now, &context.lastProc_)) < context.monitorSet()->period) {
                /* Compute how long we should sleep for next iteration. This isn't going to be perfect though */
                sleepTime = epicsMin(diff, sleepTime);
                continue;
            }

            if (!context.save()) {
                printf("pvSave: save failed\n");
                s_lastStatus = 1;
            }
            context.lastProc_ = now;
            sleepTime = epicsMin(sleepTime, context.monitorSet()->period);
        }

        /* Kick off I/O scan for status records */
        scanIoRequest(*statusIoScan());
    }
}

void pvsInitHook(initHookState state) {
    /* Create the contexts and init everything else */
    if (state == initHookAtIocBuild) {
        for (auto &ms : monitorSets) {
            SaveContext::saveContexts.emplace_back(ms.second);
        }
    }
    /* Kick off discovery of PVs */
    else if (state == initHookAfterInitDevSup) {
        printf("pvSave: Discovering PVs\n");
        for (auto &context : SaveContext::saveContexts)
            context.init();

        /* Pass 0 restore */
        for (auto &context : SaveContext::saveContexts) {
            if (context.monitorSet()->stage == state)
                context.restore();
        }
    } else if (state == initHookAfterInitDatabase) {
        /* Pass 1 restore */
        for (auto &context : SaveContext::saveContexts) {
            if (context.monitorSet()->stage == state)
                context.restore();
        }
    } else if (state == initHookAfterIocRunning) {
        /* Pass 2 restore */
        for (auto &context : SaveContext::saveContexts) {
            if (context.monitorSet()->stage == state)
                context.restore();
        }

        epicsThreadOpts opts;
        opts.joinable = false;
        opts.priority = s_configuredThreadPriority;
        opts.stackSize = epicsThreadStackMedium;
        epicsThreadCreateOpt("pvSave", pvSaveThreadProc, nullptr, &opts);
    }
}

void registerFuncs() {
    /* pvSave_CreatePvSet */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"rate", iocshArgDouble};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvSave_CreatePvSet", 2, args};
        iocshRegister(&funcDef, pvSave_CreatePvSetCallFunc);
    }

    /* pvSave_AddPvSetIO */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"ioName", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvSave_AddPvSetIO", 2, args};
        iocshRegister(&funcDef, pvSave_AddPvSetIOCallFunc);
    }

    /* pvSave_AddPvSetPv */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"pvNameRegexp", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvSave_AddPvSetPv", 2, args};
        iocshRegister(&funcDef, pvSave_AddPvSetPvCallFunc);
    }

    /* pvSave_SetPvSetRestoreStage */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"stage", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvSave_SetPvSetRestoreStage", 2, args};
        iocshRegister(&funcDef, pvSave_SetPvSetRestoreStageCallFunc);
    }

    /* pvSave_ListChannels */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"filter", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvSave_ListChannels", 2, args};
        iocshRegister(&funcDef, pvSave_ListChannelsCallFunc);
    }

    /* pvSave_AddPvSetListCallFunc */
    {
        static iocshArg arg0 = {"pvSetName", iocshArgString};
        static iocshArg arg1 = {"file", iocshArgString};
        static iocshArg arg2 = {"macroString", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1, &arg2};
        static iocshFuncDef funcDef = {"pvSave_AddPvSetList", 3, args};
        iocshRegister(&funcDef, pvSave_AddPvSetListCallFunc);
    }

    /* pvSave_ListPvSets */
    {
        static const iocshFuncDef funcDef = {"pvSave_ListPvSets", 0, nullptr};
        iocshRegister(&funcDef, pvSave_ListPvSetsCallFunc);
    }

    /* pvSave_SetThreadPriority */
    {
        static iocshArg arg0 = {"priority", iocshArgString};
        static const iocshArg* args[] = {&arg0};
        static iocshFuncDef funcDef = {"pvSave_SetThreadPriority", 1, args};
        iocshRegister(&funcDef, pvSave_SetThreadPriorityCallFunc);
    }

    /* psSave_Save */
    {
        static iocshFuncDef funcDef = {"pvSave_Save", 0, NULL};
        iocshRegister(&funcDef, pvSave_SaveCallFunc);
    }

    initHookRegister(pvsInitHook);
}

epicsExportRegistrar(registerFuncs);
