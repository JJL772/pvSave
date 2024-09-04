
#pragma once

#include <dbAccess.h>

/**
 * Simple RAII locker for DB scan locks.
 */
class dbAutoScanLock {
    dbCommon* pdb_;
public:
    dbAutoScanLock(dbCommon* pdb) :
        pdb_(pdb)
    {
        dbScanLock(pdb);
    }

    ~dbAutoScanLock()
    {
        dbScanUnlock(pdb_);
    }
};