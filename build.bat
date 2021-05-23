ECHO off
cmake .

IF %errorlevel% NEQ 0 (
	ECHO.
	ECHO [41mcmake error, compilation aborted.[0m
	ECHO.
	GOTO end
)

ECHO.
ECHO [42mRun MSBuild...[0m
ECHO.

MSBuild ALL_BUILD.vcxproj /nologo /property:Configuration=Release

:end
ECHO on