TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += pvSaveHttp

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install pvSaveHttp.dbd into <top>/dbd
DBD += pvSaveHttp.dbd

# specify all source files to be compiled and added to the library
pvSaveHttp_SRCS += httpIO.cpp

pvSaveHttp_LIBS += pvSave
pvSaveHttp_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_SYS_LIBS += curl

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

