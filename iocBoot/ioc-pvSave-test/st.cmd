#!../../bin/linux-x86_64/pvSaveTest

#===============================================================#
#
# pvSave example IOC startup script
#
#===============================================================#

< envPaths

cd "${TOP}"

#===============================================================#
#
# Common IOC setup
#
#===============================================================#

# Register all support components
dbLoadDatabase "dbd/pvSaveTest.dbd"
pvSaveTest_registerRecordDeviceDriver pdbbase

# Load record instances
dbLoadRecords("db/pvSaveTest.db","P=myioc:")

#===============================================================#
#
# pvSave Configuration
#
#===============================================================#

# Load pvSave status records
dbLoadRecords("db/SaveStatus.db","P=myioc:")

# Create a new PV set to monitor for changes
pvSave_CreatePvSet("test1", 10.0)

# Create an IO instance for saving JSON to the file system
pvSave_ConfigureFileSystemIO("fsio1", "test.jsav", "json")
#pvSave_ConfigureHTTPIO("httpio1", "http://localhost:5000")
#pvSave_AddPvSetPV("test1", "test001.VAL")
#pvSave_AddPvSetPV("test1", "test002.VAL")

# Add a list of PVs to save
pvSave_AddPvSetList("test1", "${TOP}/iocBoot/${IOC}/pvlist.pvl", "P=myioc:")

# Add an IO backend to save/load them
pvSave_AddPvSetIO("test1", "fsio1")

# Set the restore stage. Same as autosave pass 1
pvSave_SetPvSetRestoreStage("test1", "1")

#===============================================================#

cd "${TOP}/iocBoot/${IOC}"
iocInit
