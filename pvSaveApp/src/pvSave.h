
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

}