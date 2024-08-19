
#include "pvsave/pvSave.h"

pvsave::pvSaveIO::pvSaveIO(const char* instName) :
    instName_(instName)
{
    pvsave::ioBackends().insert({instName, this});
}

pvsave::pvSaveIO::~pvSaveIO()
{
    auto it = pvsave::ioBackends().find(instName_);
    if (it != pvsave::ioBackends().end())
        pvsave::ioBackends().erase(it);
}
