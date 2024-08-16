
#include "iocsh.h"
#include "epicsExport.h"

#include "pv/pvDatabase.h"
#include "pv/pvData.h"

using namespace epics::pvDatabase;

static void createMonitorSet(const char* pattern)
{
    PVDatabase::getMaster()->getRecordNames();
}

static void listRecordsCallFunc(const iocshArgBuf* buf)
{
    auto names = PVDatabase::getMaster()->getRecordNames();
    for (const auto& s : names->view()) {
        printf("%s\n", s.c_str());
    }
}

void registerFuncs()
{
    {
        static iocshFuncDef funcDef = {"pvsListRecords", 0, NULL };
        iocshRegister(&funcDef, listRecordsCallFunc);
    }
}

epicsExportRegistrar(registerFuncs);