TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = pvSaveTest
# pvSaveTest.dbd will be created and installed
DBD += pvSaveTest.dbd

# pvSaveTest.dbd will be made up from these files:
pvSaveTest_DBD += base.dbd

# Include dbd files from all support applications:
pvSaveTest_DBD += pvSave.dbd pvSaveHttp.dbd
#pvSaveTest_DBD += pvxsIoc.dbd

# Add all the support libraries needed by this IOC
pvSaveTest_LIBS += pvSave
pvSaveTest_LIBS += pvSaveHttp

# pvSaveTest_registerRecordDeviceDriver.cpp derives from pvSaveTest.dbd
pvSaveTest_SRCS += pvSaveTest_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
pvSaveTest_SRCS_DEFAULT += pvSaveTestMain.cpp
pvSaveTest_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#pvSaveTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
pvSaveTest_LIBS += $(EPICS_BASE_IOC_LIBS)
pvSaveTest_LIBS += $(EPICS_BASE_PVA_CORE_LIBS)
#pvSaveTest_LIBS += pvxsIoc

# FIXME: remove
USR_SYS_LIBS += curl

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

