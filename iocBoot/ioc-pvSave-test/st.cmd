#!../../bin/linux-x86_64/pvSaveTest

#- You may have to change pvSaveTest to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/pvSaveTest.dbd"
pvSaveTest_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords("db/pvSaveTest.db","PFX=myioc")

pvSave_CreatePvSet("test1", 10.0)

pvSave_ConfigureFileSystemIO("fsio1", "test.sav", "text")
#pvSave_ConfigureHTTPIO("httpio1", "http://localhost:5000")
#pvSave_AddPvSetPV("test1", "test001.VAL")
#pvSave_AddPvSetPV("test1", "test002.VAL")
pvSave_AddPvSetList("test1", "${TOP}/iocBoot/${IOC}/pvlist.pvl", "PFX=myioc")
pvSave_AddPvSetIO("test1", "fsio1")

pvSave_SetPvSetRestoreStage("test1", "1")


cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=jeremy"
