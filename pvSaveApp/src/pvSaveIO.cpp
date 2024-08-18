
#include "pvSaveIO.h"
#include "pvSave.h"

pvsave::pvSaveIO::pvSaveIO(const char* instName) :
    instName_(instName)
{
    pvsave::ioBackends().insert({instName, this});
}

pvsave::pvSaveIO::~pvSaveIO()
{
}