:@echo off
:.tab=4

if "%1"=="-uac" (
	cd /d "%~dp0"
	taskkill /f /im k15lowpow.exe
	copy lowpow.cfg %temp%\lowpow.cfg
	echo Debug=1 >>%temp%\lowpow.cfg
	start /high /wait k15lowpow %temp%\lowpow.cfg
	start /high k15lowpow lowpow.cfg
) else (
	%USERPROFILE%\OneDrive\bin\scripter.wsf -u2 -S6 %0 -uac
)
