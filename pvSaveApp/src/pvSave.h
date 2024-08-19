
#pragma once

#include <string>
#include <unordered_map>

namespace pvsave {
    class pvSaveIO;

    /**
     * \brief Returns a reference to the global list of IO backends.
     * In general, this is for internal use and is automatically called by the pvSaveIO constructor.
     */
    std::unordered_map<std::string, pvsave::pvSaveIO*> &ioBackends();

    /**
     * Abstract interface implemented by all data source 'plugins'
     * Data sources provided PV data from an arbitrary backend such as the IOC DB, ChannelAccess or PVXS. 
     */
    class DataSource {
    public:

        /**
         * \brief Init the data source
         */
        virtual bool init() = 0;

        /**
         * \brief Called to discover PVs. This should be run asynchronously
         */
        virtual void discoverPvs() = 0;
    };
}