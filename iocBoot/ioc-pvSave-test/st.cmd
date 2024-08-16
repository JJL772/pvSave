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

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=jeremy"
