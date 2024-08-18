#!../../bin/linux-x86_64/pvSaveTest

#- You may have to change pvSaveTest to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/pvSaveTest.dbd"
pvSaveTest_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords("db/pvSaveTest.db","user=jeremy")


pvsCreateMonitorSet("test1", 10.0)

pvsConfigureFileSystemIO("fsio1", "test.sav", "text")
pvsAddMonitorSetPV("test1", "test001.VAL")
pvsAddMonitorSetIO("test1", "fsio1")

pvsSetMonitorSetRestoreStage("test1", "0")


cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=jeremy"
