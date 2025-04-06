/**
 * ----------------------------------------------------------------------------
 * Company    : SLAC National Accelerator Laboratory
 * ----------------------------------------------------------------------------
 * Description: Device support types to display status and/or control pvSave
 *  remotely.
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

#define USE_TYPED_DSET

#include <string.h>
#include <stdio.h>

#include "epicsExport.h"
#include "longinRecord.h"
#include "longoutRecord.h"
#include "stringinRecord.h"
#include "mbbiRecord.h"
#include "dbCommon.h"
#include "epicsString.h"
#include "epicsStdlib.h"
#include "epicsAssert.h"
#include "dbScan.h"

#include "common.h"

using namespace pvsave;

//-------------------------------------------------------------------------//
// Common to all records
//-------------------------------------------------------------------------//

IOSCANPVT* pvsave::statusIoScan()
{
    static bool initted = false;
    static IOSCANPVT ios;
    if (!initted) {
        scanIoInit(&ios);
        initted = true;
    }
    return &ios;
}

//-------------------------------------------------------------------------//
// Save Status Device Support
//-------------------------------------------------------------------------//

static long saveStatus_init(int after);
static long saveStatus_init_record(dbCommon* prec);
static long saveStatus_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt);
static long saveStatus_read(longinRecord* prec);

struct SaveStatusDpvt {
    enum {
        Status,
        LastSaved,
    } type;
    epicsInt32 monitorSet = -1;
};

longindset devSaveStatusDevSup = {
    .common = {
        .number = 5,
        .report = nullptr,
        .init = saveStatus_init,
        .init_record = saveStatus_init_record,
        .get_ioint_info = saveStatus_get_ioint_info,
    },
    .read_longin = saveStatus_read,
};

epicsExportAddress(dset, devSaveStatusDevSup);

static long saveStatus_init(int after)
{
    return 0;
}

static long saveStatus_init_record(dbCommon* prec)
{
    constexpr const char* funcName = "saveStatus_init_record";

    auto* pr = reinterpret_cast<longinRecord*>(prec);
    auto* dpvt = new SaveStatusDpvt;
    pr->dpvt = dpvt;

    auto* instio = pr->inp.value.instio.string;
    if (!epicsStrnCaseCmp(instio, "status", 6)) {
        // Try to extract a number representing the monitorset/context to monitor
        if (*(instio+6)) {
            if (epicsParseInt32(instio+6, &dpvt->monitorSet, 10, nullptr) != 0) {
                printf("%s: unable to parse instio string '%s'\n", funcName, instio);
                pr->dpvt = nullptr;
                delete dpvt;
                return -1;
            }
        }
        dpvt->type = SaveStatusDpvt::Status;
    }
    else if (!epicsStrCaseCmp(instio, "lastSaved")) {
        dpvt->type = SaveStatusDpvt::LastSaved;
    }
    else {
        printf("%s: Invalid INST_IO parameter '%s'\n", funcName, instio);
        delete dpvt;
        pr->dpvt = nullptr;
        return -1;
    }

    return 0;
}

static long saveStatus_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt)
{
    *ppvt = *statusIoScan();
    return 0;
}

static long saveStatus_read(longinRecord* prec)
{
    auto* pr = reinterpret_cast<longinRecord*>(prec);
    auto* dpvt = static_cast<SaveStatusDpvt*>(pr->dpvt);

    switch (dpvt->type) {
    case SaveStatusDpvt::Status:
        break;
    case SaveStatusDpvt::LastSaved:
        pr->val = lastProcTime(dpvt->monitorSet).secPastEpoch;
        break;
    default:
        assert(0);
    }

    return 0;
}

//-------------------------------------------------------------------------//
// Save Control Device Support
//-------------------------------------------------------------------------//

static long saveControl_init(int after);
static long saveControl_init_record(dbCommon* prec);
static long saveControl_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt);
static long saveControl_write(longoutRecord* prec);

struct SaveControlDpvt {
    enum {
        SaveAll,
    } type;
};

longoutdset devSaveControlDevSup = {
    .common = {
        .number = 5,
        .report = nullptr,
        .init = saveControl_init,
        .init_record = saveControl_init_record,
        .get_ioint_info = saveControl_get_ioint_info,
    },
    .write_longout = saveControl_write,
};

epicsExportAddress(dset, devSaveControlDevSup);

static long saveControl_init(int after)
{
    return 0;
}

static long saveControl_init_record(dbCommon* prec)
{
    constexpr const char* funcName = "saveControl_init_record";

    auto* dpvt = new SaveControlDpvt;
    auto* pr = reinterpret_cast<longoutRecord*>(prec);
    pr->dpvt = dpvt;
    auto* instio = pr->out.value.instio.string;

    if (!epicsStrCaseCmp(instio, "saveAll")) {
        dpvt->type = SaveControlDpvt::SaveAll;
    }
    else {
        printf("%s: Invalid INST_IO parameter '%s'\n", funcName, instio);
        delete dpvt;
        pr->dpvt = nullptr;
        return -1;
    }
    return 0;
}

static long saveControl_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt)
{
    return 0;
}

static long saveControl_write(longoutRecord* prec)
{
    auto* dpvt = static_cast<SaveControlDpvt*>(prec->dpvt);
    switch(dpvt->type) {
    case SaveControlDpvt::SaveAll:
        printf("Saving from PV write\n");
        pvsave::saveAllNow();
        break;
    default:
        assert(0);
    }

    /* Reset back to nil */
    prec->val = 0;

    return 0;
}

//-------------------------------------------------------------------------//
// (More) Save Status Device Support
//-------------------------------------------------------------------------//

static long saveStatusMbbi_init(int after);
static long saveStatusMbbi_init_record(dbCommon* prec);
static long saveStatusMbbi_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt);
static long saveStatusMbbi_read(mbbiRecord* prec);

struct SaveStatusMbbiDpvt {
    enum {
        Status,
    } type;
    epicsInt32 monitorSet = -1;
};

mbbidset devSaveStatusMbbiDevSup = {
    .common = {
        .number = 5,
        .report = nullptr,
        .init = saveStatusMbbi_init,
        .init_record = saveStatusMbbi_init_record,
        .get_ioint_info = saveStatusMbbi_get_ioint_info,
    },
    .read_mbbi = saveStatusMbbi_read
};

epicsExportAddress(dset, devSaveStatusMbbiDevSup);

static long saveStatusMbbi_init(int after)
{
    return 0;
}

static long saveStatusMbbi_init_record(dbCommon* prec)
{
    constexpr const char* funcName = "saveStatusMbbi_init_record";

    auto* dpvt = new SaveStatusMbbiDpvt;
    prec->dpvt = dpvt;
    
    auto* pr = reinterpret_cast<mbbiRecord*>(prec);
    auto* instio = pr->inp.value.instio.string;

    const size_t saveStatusLen = strlen("saveStatus");
    if (!epicsStrnCaseCmp(instio, "saveStatus", saveStatusLen)) {
        // Extract monitor set we want (if any)
        if (*(instio+saveStatusLen)) {
            if (epicsParseInt32(instio+saveStatusLen, &dpvt->monitorSet, 10, nullptr) != 0) {
                delete dpvt;
                pr->dpvt = nullptr;
                return -1;
            }
        }
        dpvt->type = SaveStatusMbbiDpvt::Status;
    }
    else {
        printf("%s: Invalid INST_IO parameter '%s'\n", funcName, instio);
        delete dpvt;
        pr->dpvt = nullptr;
        return -1;
    }

    return 0;
}

static long saveStatusMbbi_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt)
{
    *ppvt = *statusIoScan();
    return 0;
}

static long saveStatusMbbi_read(mbbiRecord* prec)
{
    auto* dpvt = static_cast<SaveStatusMbbiDpvt*>(prec->dpvt);
    if (!dpvt)
        return -1;

    switch(dpvt->type) {
    case SaveStatusMbbiDpvt::Status:
        prec->val = lastStatus(dpvt->monitorSet);
        break;
    default:
        assert(0);
    }
    return 0;
}

//-------------------------------------------------------------------------//
// (More) Save Status Device Support
//-------------------------------------------------------------------------//

static long saveStatusStr_init(int after);
static long saveStatusStr_init_record(dbCommon* prec);
static long saveStatusStr_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt);
static long saveStatusStr_read(stringinRecord* prec);

struct SaveStatusStrDpvt {
    enum {
        LastSaved,
    } type;
    epicsInt32 monitorSet = -1;
};

stringindset devSaveStatusStrDevSup = {
    .common = {
        .number = 5,
        .report = nullptr,
        .init = saveStatusStr_init,
        .init_record = saveStatusStr_init_record,
        .get_ioint_info = saveStatusStr_get_ioint_info,
    },
    .read_stringin = saveStatusStr_read
};

epicsExportAddress(dset, devSaveStatusStrDevSup);

static long saveStatusStr_init(int after)
{
    return 0;
}

static long saveStatusStr_init_record(dbCommon* prec)
{
    constexpr const char* funcName = "saveStatusStr_init_record";

    auto* dpvt = new SaveStatusStrDpvt;
    prec->dpvt = dpvt;

    auto* pr = reinterpret_cast<stringinRecord*>(prec);
    auto* instio = pr->inp.value.instio.string;
    const size_t lastSavedLen = strlen("lastSaved");
    if (!epicsStrnCaseCmp(instio, "lastSaved", lastSavedLen)) {
        if (*(instio + lastSavedLen)) {
            if (epicsParseInt32(instio+lastSavedLen, &dpvt->monitorSet, 10, nullptr) != 0) {
                printf("%s: Invalid INST_IO string '%s'\n", funcName, instio);
                delete dpvt;
                pr->dpvt = nullptr;
                return -1;
            }
        }
        dpvt->type = SaveStatusStrDpvt::LastSaved;

        strcpy(pr->val, "Never");
        pr->udf = FALSE;
    }
    else {
        delete dpvt;
        printf("%s: Invalid INST_IO string '%s'\n", funcName, instio);
        pr->dpvt = nullptr;
        return -1;
    }

    return 0;
}

static long saveStatusStr_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt)
{
    if (!precord->dpvt)
        return -1;

    *ppvt = *statusIoScan();
    return 0;
}

static long saveStatusStr_read(stringinRecord* prec)
{
    auto* dpvt = static_cast<SaveStatusStrDpvt*>(prec->dpvt);

    switch(dpvt->type) {
    case SaveStatusStrDpvt::LastSaved:
    {
        char time[MAX_STRING_SIZE] = {0};
        auto ts = lastProcTime(dpvt->monitorSet);
        epicsTimeToStrftime(time, sizeof(time), "%c", &ts);
        strncpy(prec->val, time, MAX_STRING_SIZE-1);
        prec->val[MAX_STRING_SIZE-1] = 0;
        break;
    }
    default:
        assert(0);
    }

    prec->udf = FALSE;
    return 0;
}