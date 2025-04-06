/**
 * ----------------------------------------------------------------------------
 * Company    : SLAC National Accelerator Laboratory
 * ----------------------------------------------------------------------------
 * Description: Data source interface that interacts with the EPICS DB
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
#include "dbAccess.h"
#include "dbAddr.h"
#include "dbCommon.h"
#include "dbStaticLib.h"
#include "epicsStdio.h"

#include "common.h"
#include "pvsave/pvSave.h"

namespace pvsave
{

/**
 * Implements a data source for interacting with EPICS V3 DB
 */
class DataSourceCA : public DataSource
{
public:
    bool init() override;
    void connect(const std::vector<std::string> &pvList, std::vector<Channel> &outChannels) override;
    void put(const Channel &channel, const Data &pvData) override;
    void get(const Channel &channel, Data &pvData) override;

private:
    struct ContextData {
        dbAddr addr = {};
        DBENTRY entry = {};
    };
    std::vector<ContextData> addrs_;
};

bool DataSourceCA::init() { return true; }

Data dataFromDbfType(short type)
{
    Data d;
    switch (type) {
    case DBF_STRING:
        d.construct<std::string>();
        return d;
    case DBF_CHAR:
        d.construct<int8_t>();
        return d;
    case DBF_UCHAR:
        d.construct<uint8_t>();
        return d;
    case DBF_SHORT:
        d.construct<int16_t>();
        return d;
    case DBF_USHORT:
        d.construct<uint16_t>();
        return d;
    case DBF_LONG:
        d.construct<int32_t>();
        return d;
    case DBF_ULONG:
        d.construct<uint32_t>();
        return d;
    case DBF_INT64:
        d.construct<int64_t>();
        return d;
    case DBF_UINT64:
        d.construct<uint64_t>();
        return d;
    case DBF_FLOAT:
        d.construct<float>();
        return d;
    case DBF_DOUBLE:
        d.construct<double>();
        return d;
    case DBF_ENUM:
        d.construct<int>();
        return d;
    case DBF_MENU:
        d.construct<int>();
        return d;
    case DBF_DEVICE:
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
    default:
        return {};
    }
}

void DataSourceCA::connect(const std::vector<std::string> &pvList, std::vector<Channel> &outChannels)
{
    addrs_.resize(pvList.size());
    for (size_t i = 0; i < pvList.size(); ++i) {
        auto &pv = pvList[i];
        if (dbNameToAddr(pv.c_str(), &addrs_[i].addr) != 0) {
            printf("Failed to connect channel %s\n", pv.c_str());
            addrs_[i] = {};
        } else {
            // printf("Connected %s\n", pv.c_str());
            dbInitEntry(pdbbase, &addrs_[i].entry);
            outChannels.push_back({pv, &addrs_[i]});
        }
    }
    printf("Connected %zu out of %zu PVs (%2.f%%)\n", 
        outChannels.size(), pvList.size(), 100.f * float(outChannels.size()) / pvList.size());
}

void DataSourceCA::put(const Channel &channel, const Data &data)
{
    auto *pdb = static_cast<dbAddr *>(channel.contextData);

    printf("put attempt for %s\n", channel.channelName.c_str());

    /** Channel not connected or no data to restore */
    if (!pdb || data.is<void>()) {
        return;
    }

    auto *pctx = static_cast<ContextData *>(channel.contextData);
    if (!pctx->entry.pdbbase) {
        dbInitEntryFromAddr(pdb, &pctx->entry);
        dbInitEntry(pdbbase, &pctx->entry);
    }

    long result;

    dbAutoScanLock al(pdb->precord);

    /** Special handling for string since we cannot directly memcpy std::string in there */
    if (pdb->dbr_field_type == DBR_STRING) {
        auto val = data.value<std::string>();
        if ((result = dbPutField(pdb, pdb->dbr_field_type, val.c_str(), val.size()))) {
            printf("DataSourceCA::put: dbPutField() failed: %ld\n", result);
        }
    } else {
        if ((result = dbPutField(pdb, pdb->dbr_field_type, data.data(), 1)) != 0) {
            printf("DataSourceCA::put: dbPutField() failed: %ld\n", result);
            return;
        }
    }
}

void DataSourceCA::get(const Channel &channel, Data &data)
{
    auto *pdb = static_cast<ContextData *>(channel.contextData);
    data = dataFromDbfType(pdb->addr.dbr_field_type);

    long result;

    dbAutoScanLock al(pdb->addr.precord);

    /** Special handling for string fields */
    if (pdb->addr.dbr_field_type == DBR_STRING) {
        char buf[MAX_STRING_SIZE + 1];
        buf[0] = 0;

        long req = sizeof(buf);
        if ((result = dbGetField(&pdb->addr, pdb->addr.dbr_field_type, buf, nullptr, &req, nullptr)) != 0) {
            printf("DataSourceCA::get: dbGet() failed: %li\n", result);
            return;
        }

        *data.get<std::string>() = buf;
    } else {
        if ((result = dbGetField(&pdb->addr, pdb->addr.dbr_field_type, data.data(), nullptr, nullptr, nullptr)) != 0) {
            printf("DataSourceCA::get: dbGet() failed: %li\n", result);
            return;
        }
    }
}

DataSource *createDataSourceCA() { return new DataSourceCA(); }

} // namespace pvsave