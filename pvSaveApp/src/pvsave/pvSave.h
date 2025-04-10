/**
 * ----------------------------------------------------------------------------
 * Company    : SLAC National Accelerator Laboratory
 * ----------------------------------------------------------------------------
 * Description: pvSave public API
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
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <stdint.h>

#include "pvsave/variant.h"

namespace pvsave {
    class SaveRestoreIO;
    class DataSource;

    /**
     * \brief Status error codes. These are returned by the I/O backends when a save fails
     */
    enum IOErrorCodes {
        S_io_ok = 0,              //< No error
        S_io_err,                 //< Generic IO error
        S_io_noAccess,            //< EACCESS (No access)
        S_io_noEntry,             //< ENOENT (when reading)
    };

    /**
     * \brief Returns a reference to the global list of IO backend instances.
     * In general, this is for internal use and is automatically called by the SaveRestoreIO constructor.
     */
    std::unordered_map<std::string, pvsave::SaveRestoreIO*> &ioBackends();

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
        std::string
    >;


    /**
     * Abstract interface implemented by all data sources
     * Data sources provide PV data from an arbitrary backend such as the IOC DB, ChannelAccess or PVXS.
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
    * SaveRestoreIO is the base class for all I/O readers/writers used by pvSave
    * It provides a subset of operations
    */
    class SaveRestoreIO {
    public:
        SaveRestoreIO() = delete;
        SaveRestoreIO(const char* instName);
        virtual ~SaveRestoreIO();

        enum Flags
        {
            /**
             * \brief Supports PV restores
             */
            Read    = (1<<0),

            /**
             * \brief Supports PV saves
             */
            Write   = (1<<1),
        };

        /**
         * \brief Returns the flags this IO backend supports
         */
        virtual uint32_t flags() const = 0;

        /** 
        * \brief Begins a write transaction
        * \returns True on success
        */
        virtual bool beginWrite() = 0;

        /** 
         * \brief Writes a single channel's data.
         * \param channel The channel description
         * \param data The data to write for this channel
         * \returns True on success
         */
        virtual bool writeData(const DataSource::Channel& channel, const Data& data) = 0;

        /**
        * \brief Ends a write transaction
        * \returns True on success
        */
        virtual bool endWrite() = 0;

        /**
        * \brief Begins a read transaction
        * \returns True on success
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
        * \returns True on success
        */
        virtual bool endRead() = 0;

        /**
        * \brief Display info about this IO instance to the stream
        * \param fp Stream to fprintf to
        * \param indent Indentation level (measured in number of spaces) to display with. Just a formatting helper
        */
        virtual void report(FILE* fp, int indent) = 0;

        /**
         * \brief Returns the name of this instance
         */
        const std::string& instanceName() const { return instName_; }

    protected:
        std::string instName_;
    };
}