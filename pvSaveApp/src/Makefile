TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += pvSave

# install pvSave.dbd into <top>/dbd
DBD += pvSave.dbd

# specify all source files to be compiled and added to the library
pvSave_SRCS += pvSave.cpp
pvSave_SRCS += pvSaveIO.cpp
pvSave_SRCS += fileSystemIO.cpp
pvSave_SRCS += serialize.cpp
pvSave_SRCS += dataSourceDb.cpp
pvSave_SRCS += statusControl.cpp

ifdef PVXS_MAJOR_VERSION
pvSave_LIBS += pvxs
endif
pvSave_LIBS += $(EPICS_BASE_IOC_LIBS)
pvSave_LIBS += $(EPICS_BASE_PVA_CORE_LIBS)

INC += pvsave/pvSave.h
INC += pvsave/serialize.h
INC += pvsave/pvxsSerialize.h
INC += pvsave/utility.h
INC += pvsave/variant.h

USR_CXXFLAGS+=-std=c++14

ifdef PVXS_MAJOR_VERSION
USR_CXXFLAGS += -DHAVE_PVXS=1
endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

