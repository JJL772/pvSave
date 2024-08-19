
#pragma once

#include <string>
#include <utility>
#include <stdint.h>

#include "pvxs/data.h"

namespace pvsave {

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

    virtual bool writeData(const std::string* pvNames, const pvxs::Value* pvValues, size_t pvCount) = 0;

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

    virtual bool readData(std::vector<std::string>& pvNames, std::vector<pvxs::Value>& pvValues) = 0;

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

} // namespace pvsave