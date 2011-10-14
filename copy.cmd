del /f /q *.ilk
ren QQSpy.exe QQMon.exe
ren QQSpy.pdb QQMon.pdb
ren QQSpyStart.exe QQStart.exe
ren QQSpyStart.pdb QQStart.pdb
copy ..\QQMon.start .
copy ..\QQSpy\QQSpy.cpp .
copy ..\QQSpyStart\QQSpyStart.cpp .
