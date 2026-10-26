Set WshShell = CreateObject("WScript.Shell")
strDir = CreateObject("Scripting.FileSystemObject").GetParentFolderName(WScript.ScriptFullName)
WshShell.CurrentDirectory = strDir & "\build\Release"
WshShell.Run "cmd /c ..\JUGAR_16_9.bat", 0, False
