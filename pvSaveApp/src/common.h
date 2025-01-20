//////////////////////////////////////////////////////////////////////////////
// This file is part of 'pvSave'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'pvSave', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <dbAccess.h>

namespace pvsave {

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
}

/**
 * Simple RAII locker for DB scan locks.
 */
class dbAutoScanLock {
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
