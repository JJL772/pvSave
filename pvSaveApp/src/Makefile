TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += pvSave

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install pvSave.dbd into <top>/dbd
DBD += pvSave.dbd

# specify all source files to be compiled and added to the library
#pvSave_SRCS += xxx

pvSave_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

