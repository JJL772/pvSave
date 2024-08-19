
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include <list>

#include "pvSave.h"
#include "pvSaveIO.h"

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
    class pvSaveContext *context;
};

std::unordered_map<std::string, std::shared_ptr<MonitorSet>> monitorSets;

/**
 * \brief Represents a PV data provider
 */
class pvSaveContext {
public:
    pvSaveContext(std::shared_ptr<MonitorSet> set)
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
        std::vector<pvsave::Data> data;
        pvsave::dataSource()->get(channels_, data);

        /* Forward the data to each of the IO handlers */
        for (auto &io : monitorSet_->io) {
            if (io->flags() & pvsave::pvSaveIO::Write) {
                if (!io->beginWrite()) {
                    printf("pvSave: io->beginWrite: save failed\n");
                    continue;
                }

                if (!io->writeData(channels_, data, data.size())) {
                    printf("pvSave: io->writeData: save failed\n");
                    /* Fallthrough as we want the IO handler to do cleanup */
                }

                if (!io->endWrite()) {
                    printf("pvSave: io->endWrite: save failed\n");
                }
            }
        }

        return true;
    }

private:
    bool restore(pvsave::pvSaveIO *io) {
        if (!(io->flags() & pvsave::pvSaveIO::Read))
            return false;

        if (!io->beginRead()) {
            printf("pvSave: io->beginRead: restore failed\n");
            return false;
        }

        std::vector<std::string> pvNames;
        std::vector<pvsave::Data> pvValues;
        if (!io->readData(pvNames, pvValues)) {
            printf("pvSave: io->readData: restore failed\n");
            /* Fallthrough to allow cleanup */
        }

        if (!io->endRead()) {
            printf("pvSave: io->endRead: restore failed\n");
        }

        pvsave::dataSource()->put(channels_, pvValues);

        return true;
    }

public:
    bool restore() {
        bool restoredAny = false;
        for (auto &io : monitorSet_->io) {
            if (restore(io)) {
                restoredAny = true;
                break;
            }
        }

        if (!restoredAny) {
            printf("%s: restore failed: no backend was able to restore\n", "pvSaveContext::restore");
            return false;
        }
        return true;
    }

    /** Returns the channels */
    inline const std::vector<pvsave::DataSource::Channel> &channels() const { return channels_; }

    /** Returns the monitor set we were configured with */
    inline const std::shared_ptr<MonitorSet> &monitorSet() const { return monitorSet_; }

    static std::list<pvSaveContext> saveContexts;

    epicsTimeStamp lastProc_;

protected:
    std::shared_ptr<MonitorSet> monitorSet_;
    std::vector<pvsave::DataSource::Channel> channels_;
    bool pendingRestore_ = false;
};

std::list<pvSaveContext> pvSaveContext::saveContexts;

static std::shared_ptr<MonitorSet> findMonitorSet(const char *name) {
    auto it = monitorSets.find(name);
    if (it == monitorSets.end())
        return {};
    return it->second;
}

static void pvsCreateMonitorSetCallFunc(const iocshArgBuf *buf) {
    const char *name = buf[0].sval;
    double rate = buf[1].dval;

    if (rate < 10.0) {
        printf("pvsCreateMonitorSet: rate must be >= 10 (%f)\n", rate);
        return;
    }

    if (!name) {
        printf("pvsCreateMonitorSet expects 'name' parameter\n");
        return;
    }

    monitorSets.insert({name, std::make_shared<MonitorSet>(name, rate)});
}

static void pvsAddMonitorSetIOCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvsAddMonitorSetIO";
    const char *name = buf[0].sval;
    const char *ioName = buf[1].sval;

    if (!name || !ioName) {
        printf("%s: expected 'name' and 'ioName' parameter\n", funcName);
        return;
    }

    auto ms = findMonitorSet(name);
    if (!ms) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
        return;
    }

    if (auto it = pvsave::ioBackends().find(ioName); it != pvsave::ioBackends().end()) {
        ms->io.push_back(it->second);
    } else {
        printf("%s: No such IO backend '%s'\n", funcName, ioName);
        return;
    }
}

static void pvsAddMonitorSetPVCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvsAddMonitorSetPV";
    const char *name = buf[0].sval;
    const char *pvPattern = buf[1].sval;

    if (!name || !pvPattern) {
        printf("%s: expected 'name' and 'pvPattern' parameter\n", funcName);
        return;
    }

    auto mset = findMonitorSet(name);
    if (!mset) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
        return;
    }
    mset->pvList.push_back(pvPattern);
}

static void pvsSetMonitorSetRestoreStageCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvsSetMonitorSetRestoreStage";
    const char *name = buf[0].sval;
    const char *stage = buf[1].sval;

    if (!name || !stage) {
        printf("%s: expected 'name' and 'stage' parameter\n", funcName);
        return;
    }

    auto mset = findMonitorSet(name);
    if (!mset) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
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
            mset->stage = initHookAfterFinishDevSup;
            break;
        case 2:
            mset->stage = initHookAfterIocRunning;
            break;
        default:
            printf("%s: invalid stage '%d': 0 or 1 allowed\n", funcName, st);
            return;
        }
    }
    /* May also be a string */
    else {
        if (!epicsStrCaseCmp(stage, "AfterInitDevSup")) {
            mset->stage = initHookAfterInitDevSup;
        } else if (!epicsStrCaseCmp(stage, "AfterInitDatabase")) {
            mset->stage = initHookAfterFinishDevSup;
        } else if (!epicsStrCaseCmp(stage, "AfterIocRunning")) {
            mset->stage = initHookAfterIocRunning;
        } else {
            printf("%s: invalid stage '%s': 'AfterInitDevSup' (0), 'AfterInitDatabase' (1) allowed\n", funcName, stage);
        }
    }
}

static void pvsMonitorSetListCallFunc(const iocshArgBuf *buf) {
    for (auto &pair : monitorSets) {
        printf("%s: %lu PVs\n", pair.first.c_str(), pair.second->pvList.size());
        printf("  IO ports:\n");
        for (int i = 0; i < pair.second->io.size(); ++i) {
            printf("   %d:\n", i);
            pair.second->io[i]->report(stdout, 5);
        }
    }
}

static void pvsMonitorSetListChannelsCallFunc(const iocshArgBuf *buf) {
    constexpr const char *funcName = "pvsMonitorSetListChannels";
    const char *name = buf[0].sval;
    const char *filter = buf[1].sval;

    if (!name) {
        printf("%s: expected 'name' parameter\n", funcName);
        return;
    }

    auto mset = findMonitorSet(name);
    if (!mset) {
        printf("%s: invalid monitor set name '%s'\n", funcName, name);
        return;
    }

    bool onlyConnected = filter ? !epicsStrCaseCmp(filter, "connected") : false;
    bool onlyDisconnected = filter ? !epicsStrCaseCmp(filter, "disconnected") : false;

#if 0
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

static void pvsThreadProc(void *data) {
    epicsTimeStamp startTime;
    epicsTimeGetCurrent(&startTime);

    double sleepTime = 30.f;
    for (auto &context : pvSaveContext::saveContexts)
        sleepTime = epicsMin(context.monitorSet()->period, sleepTime);

    while (1) {
        epicsThreadSleep(sleepTime);
        sleepTime = 30.f;

        epicsTimeStamp now;
        epicsTimeGetCurrent(&now);

        for (auto &context : pvSaveContext::saveContexts) {
            double diff;
            if ((diff = epicsTimeDiffInSeconds(&now, &context.lastProc_)) < context.monitorSet()->period) {
                /* Compute how long we should sleep for next iteration. This isn't going to be perfect though */
                sleepTime = epicsMin(diff, sleepTime);
                continue;
            }

            printf("pvSave: saving...\n");
            if (!context.save())
                printf("pvSave: save failed\n");
            context.lastProc_ = now;
            sleepTime = epicsMin(sleepTime, context.monitorSet()->period);
        }
    }
}

void pvsInitHook(initHookState state) {
    /* Create the contexts and init everything else */
    if (state == initHookAtIocBuild) {
        for (auto &ms : monitorSets) {
            pvSaveContext::saveContexts.emplace_back(ms.second);
        }
    }
    /* Kick off discovery of PVs */
    else if (state == initHookAfterInitDevSup) {
        printf("pvSave: Discovering PVs\n");
        for (auto &context : pvSaveContext::saveContexts)
            context.init();

        /* Pass 0 restore */
        for (auto &context : pvSaveContext::saveContexts) {
            if (context.monitorSet()->stage == state)
                context.restore();
        }
    } else if (state == initHookAfterFinishDevSup) {
        /* Pass 1 restore */
        for (auto &context : pvSaveContext::saveContexts) {
            if (context.monitorSet()->stage == state)
                context.restore();
        }
    } else if (state == initHookAfterIocRunning) {
        for (auto &context : pvSaveContext::saveContexts) {
            if (context.monitorSet()->stage == state)
                context.restore();
        }

        epicsThreadOpts opts;
        opts.joinable = false;
        opts.priority = epicsThreadPriorityMedium;
        opts.stackSize = epicsThreadStackMedium;
        epicsThreadCreateOpt("pvSave", pvsThreadProc, nullptr, &opts);
    }
}

void registerFuncs() {
    /* pvsCreateMonitorSet */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"rate", iocshArgDouble};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvsCreateMonitorSet", 2, args};
        iocshRegister(&funcDef, pvsCreateMonitorSetCallFunc);
    }

    /* pvsAddMonitorSetIO */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"ioName", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvsAddMonitorSetIO", 2, args};
        iocshRegister(&funcDef, pvsAddMonitorSetIOCallFunc);
    }

    /* pvsAddMonitorSetPV */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"pvNameRegexp", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvsAddMonitorSetPV", 2, args};
        iocshRegister(&funcDef, pvsAddMonitorSetPVCallFunc);
    }

    /* pvsSetMonitorSetRestoreStage */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"stage", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvsSetMonitorSetRestoreStage", 2, args};
        iocshRegister(&funcDef, pvsSetMonitorSetRestoreStageCallFunc);
    }

    /* pvsMonitorSetListChannels */
    {
        static iocshArg arg0 = {"setName", iocshArgString};
        static iocshArg arg1 = {"filter", iocshArgString};
        static const iocshArg *args[] = {&arg0, &arg1};
        static iocshFuncDef funcDef = {"pvsMonitorSetListChannels", 2, args};
        iocshRegister(&funcDef, pvsMonitorSetListChannelsCallFunc);
    }

    /* pvsMonitorSetList */
    {
        static const iocshFuncDef funcDef = {"pvsMonitorSetList", 0, nullptr};
        iocshRegister(&funcDef, pvsMonitorSetListCallFunc);
    }

    initHookRegister(pvsInitHook);
}

epicsExportRegistrar(registerFuncs);
