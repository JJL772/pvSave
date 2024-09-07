
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <stdint.h>

#include "pvsave/variant.h"

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
        virtual void put(const Channel& channels, const Data& pvData) = 0;

        /**
         * Get PV data
         * \param pvList List of PVs to get data for
         * \param pvData list to place the data into. Expects a 1:1 mapping of pvList <-> pvData
         */
        virtual void get(const Channel& channel, Data& pvData) = 0;
    };

    DataSource* createDataSourceCA();

    /**
    * pvSaveIO is the base class for all I/O readers/writers used by pvSave
    * It provides a subset of operations
    */
    class pvSaveIO {
    public:
        pvSaveIO() = delete;
        pvSaveIO(const char* instName);
        virtual ~pvSaveIO();

        enum Flags
        {
            Read    = (1<<0),   /** This supports data reads */
            Write   = (1<<1),   /** This supports data writes */
        };

        virtual uint32_t flags() const = 0;

        /** 
        * \brief Begins a write transaction
        * \returns False if failed
        */
        virtual bool beginWrite() = 0;

        /** 
         * \brief Write channel data to the IO backend
         * \param channels List of channels to write
         * \param pvData List of PV data. This is a 1:1 mapping to the channels
         * \param pvCount Number of items in both arrays
         */
        virtual bool writeData(const std::vector<DataSource::Channel>& channels, const std::vector<Data>& pvData, size_t pvCount) = 0;

        /**
        * \brief Ends a write transaction
        * \returns False if failed
        */
        virtual bool endWrite() = 0;


        /**
        * \brief Begins a read transaction
        * \returns False if failed
        */
        virtual bool beginRead() = 0;

        /**
         * \brief Read channel data for the specified PVs
         * \param pvNames PV names
         * \param pvValues PV values. pvValues will come in with length 0. It is up to you to add all output data here.
         */
        virtual bool readData(std::unordered_map<std::string, Data>& pvs) = 0;

        /**
        * \brief Ends a read transaction
        * \returns False if failed
        */
        virtual bool endRead() = 0;

        /**
        * \brief Display info about this IO instance to the stream
        * \param fp Stream to fprintf to
        * \param indent Indentation level (measured in number of spaces) to display with. Just a formatting helper
        */
        virtual void report(FILE* fp, int indent) = 0;

    protected:
        std::string instName_;
    };
}