/**
 * ----------------------------------------------------------------------------
 * Company    : SLAC National Accelerator Laboratory
 * ----------------------------------------------------------------------------
 * Description: Common helpers
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

#include <dbAccess.h>
#include <errlog.h>

namespace pvsave
{
    
enum ELoggingLevel
{
    LL_Trace = -2,
    LL_Debug = -1,
    LL_Info = 0,
    LL_Warn,
    LL_Err,
};

/**
 * Save NOW! Locks the save mutex
 */
void saveAllNow();

/**
 * Returns IOSCANPVT instance used by all status records
 */
IOSCANPVT* statusIoScan();

/**
 * Returns the last proc time for the specified monitor set
 * if -1, returns last proc time for any monitor set
 */
epicsTimeStamp lastProcTime(int ms = -1);

/**
 * Returns the last status for the specified monitor set
 * if -1, returns last proc time for any monitor set
 */
int lastStatus(int ms = -1);

/**
 * Current logging level
 */
extern ELoggingLevel logLevel;

} // namespace pvsave

/**
 * Logging macros.
 */
#define LOG_MSG(_level, ...) \
    do { \
        if (_level >= pvsave::logLevel) \
            errlogPrintf(__VA_ARGS__); \
    } while (0)

#define LOG_TRACE(...) LOG_MSG(LL_Trace, __VA_ARGS__)
#define LOG_WARN(...) LOG_MSG(LL_Warn, __VA_ARGS__)
#define LOG_ERR(...) LOG_MSG(LL_Err, __VA_ARGS__)
#define LOG_DBG(...) LOG_MSG(LL_Debug, __VA_ARGS__)
#define LOG_INFO(...) LOG_MSG(LL_Info, __VA_ARGS__)

/**
 * Simple RAII locker for DB scan locks.
 */
class dbAutoScanLock
{
    dbCommon* pdb_;

public:
    dbAutoScanLock(dbCommon* pdb) :
        pdb_(pdb)
    {
        if (pdb)
            dbScanLock(pdb);
    }

    ~dbAutoScanLock()
    {
        if (pdb_)
            dbScanUnlock(pdb_);
    }
};
