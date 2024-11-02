
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
        if (pdb)
            dbScanLock(pdb);
    }

    ~dbAutoScanLock()
    {
        if (pdb_)
            dbScanUnlock(pdb_);
    }
};

/**
 * Performs a trace log. If level is lower than the trace level, then we do nothing.
 */
static void traceLog(int level, const char* fmt, ...);

/**
 * Returns the current trace level
 */
static int traceLevel();