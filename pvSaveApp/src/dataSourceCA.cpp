
#include "dbAccess.h"
#include "dbAddr.h"
#include "dbCommon.h"

#include "pvSave.h"

namespace pvsave {

class DataSourceCA : public DataSource {
public:
    bool init() override;
    void connect(const std::vector<std::string>& pvList, std::vector<Channel>& outChannels) override;
    void put(const std::vector<Channel>& channels, const std::vector<Data>& pvData) override;
    void get(const std::vector<Channel>& channels, std::vector<Data>& pvData) override;
private:
    std::vector<dbAddr> addrs_;
};

bool DataSourceCA::init() {
    return true;
}

Data dataFromDbfType(short type) {
    Data d;
    switch(type) {
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
    case DBF_MENU:
    case DBF_DEVICE:
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
    default:
        return {};
    }
}

void DataSourceCA::connect(const std::vector<std::string>& pvList, std::vector<Channel>& outChannels) {
    addrs_.resize(pvList.size());
    for (size_t i = 0; i < pvList.size(); ++i) {
        auto& pv = pvList[i];
        if (dbNameToAddr(pv.c_str(), &addrs_[i]) != 0) {
            printf("Failed to connect channel %s\n", pv.c_str());
        }
        else {
            printf("Connected %s\n", pv.c_str());
            outChannels.push_back({pv, &addrs_[i]});
        }
    }
}

void DataSourceCA::put(const std::vector<Channel>& channels, const std::vector<Data>& pvData) {
    if (pvData.size() == 0) return; // FIXME
    
    for (size_t i = 0; i < channels.size(); ++i) {
        const auto& channel = channels[i];
        auto* pdb = static_cast<dbAddr*>(channel.contextData);

        /** Channel not connected or no data to restore */
        if (!pdb || pvData[i].is<void>()) {
            continue;
        }

        long result;

        /** Special handling for string since we cannot directly memcpy std::string in there */
        if (pdb->dbr_field_type == DBR_STRING) {
            auto val = pvData[i].value<std::string>();
            if ((result = dbPutField(pdb, pdb->dbr_field_type, val.c_str(), val.size()))) {
                printf("DataSourceCA::put: dbPutField() failed: %ld\n", result);    
            }
        }
        else {
            if ((result = dbPutField(pdb, pdb->dbr_field_type, pvData[i].data(), 1)) != 0) {
                printf("DataSourceCA::put: dbPutField() failed: %ld\n", result);
                continue;
            }
        }
    }
}

void DataSourceCA::get(const std::vector<Channel>& channels, std::vector<Data>& pvData) {
    pvData.resize(channels.size());

    for (size_t i = 0; i < channels.size(); ++i) {
        const auto& channel = channels[i];
        auto* pdb = static_cast<dbAddr*>(channel.contextData);
        auto& data = pvData[i];
        data = dataFromDbfType(pdb->dbr_field_type);

        long result;

        /** Special handling for string fields */
        if (pdb->dbr_field_type == DBR_STRING) {
            char buf[MAX_STRING_SIZE];
            buf[0] = 0;

            long req = sizeof(buf);
            if ((result = dbGetField(pdb, pdb->dbr_field_type, buf, nullptr, &req, nullptr)) != 0) {
                printf("DataSourceCA::get: dbGet() failed: %li\n", result);
                continue;
            }

            *data.get<std::string>() = buf;
        }
        else {
            if ((result = dbGetField(pdb, pdb->dbr_field_type, data.data(), nullptr, nullptr, nullptr)) != 0) {
                printf("DataSourceCA::get: dbGet() failed: %li\n", result);
                continue;
            }
        }
    }
}

DataSource* createDataSourceCA() {
    return new DataSourceCA();
}

}