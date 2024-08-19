
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <stdint.h>

#include "variant.hpp"

namespace pvsave {
    class pvSaveIO;
    class DataSource;

    /**
     * \brief Returns a reference to the global list of IO backend instances.
     * In general, this is for internal use and is automatically called by the pvSaveIO constructor.
     */
    std::unordered_map<std::string, pvsave::pvSaveIO*> &ioBackends();

    DataSource* dataSource();

    /** Encapsulates all of our data types */
    using Data = Variant<
        int8_t,
        uint8_t,
        int16_t,
        uint16_t,
        int32_t,
        uint32_t,
        int64_t,
        uint64_t,
        float,
        double,
        std::string>;

    /**
     * Abstract interface implemented by all data source 'plugins'
     * Data sources provided PV data from an arbitrary backend such as the IOC DB, ChannelAccess or PVXS. 
     */
    class DataSource {
    public:

        struct Channel {
            std::string channelName;
            void* contextData;
        };

        /**
         * \brief Init the data source
         */
        virtual bool init() = 0;

        /** 
         * \brief Called to connect the given channels
         * \param pvList list of PV names to connect
         * \param outChannels Output list of per-channel context data. This is passed in to put/get
         */
        virtual void connect(const std::vector<std::string>& pvList, std::vector<Channel>& outChannels) = 0;

        /**
         * Put PV data
         * \param channels List of channels to PUT
         * \param pvData the data to put into these channels. This is a 1:1 mapping between the arrays. that is, channels[1] corresponds to pvData[1] and so on
         */
        virtual void put(const std::vector<Channel>& channels, const std::vector<Data>& pvData) = 0;

        /**
         * Get PV data
         * \param pvList List of PVs to get data for
         * \param pvData list to place the data into. Expects a 1:1 mapping of pvList <-> pvData
         */
        virtual void get(const std::vector<Channel>& channels, std::vector<Data>& pvData) = 0;
    };

    DataSource* createDataSourceCA();
}