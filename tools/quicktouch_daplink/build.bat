@echo off
rem Builds DapLinkTransportTest.exe with the .NET Framework 4.x compiler (C# 5), no Visual Studio needed.
setlocal
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
if not exist "%CSC%" set CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe
cd /d "%~dp0"
"%CSC%" /nologo /target:exe /platform:anycpu /optimize+ /out:DapLinkTransportTest.exe QuickTouch\I2C\ITouchTransport.cs QuickTouch\I2C\DapLinkTransport.cs DapLinkTransportTest.cs
