@echo off
rem Builds the adaC compiler on Windows with MinGW-w64 (gcc).
rem If gcc is not in the PATH, it is looked up in GCC_DIR (edit the line below if yours is elsewhere).
rem The PATH change only lasts while this script runs: nothing in Windows is modified.
setlocal
set "GCC_DIR=C:\gccgnu\bin"

where gcc >nul 2>nul
if errorlevel 1 (
    if exist "%GCC_DIR%\gcc.exe" (
        set "PATH=%GCC_DIR%;%PATH%"
    ) else (
        echo gcc was not found in the PATH nor in "%GCC_DIR%".
        echo Edit GCC_DIR at the top of build.bat to the folder that contains gcc.exe.
        exit /b 1
    )
)

cd /d "%~dp0"
if not exist build mkdir build

gcc -O1 -o build\embed.exe src\tools\embed\embed.c || exit /b 1
build\embed.exe library > build\library_data.c || exit /b 1
gcc -O2 -std=gnu11 -Icompiler -o adac.exe compiler\adac_data_structures\data_structures.c compiler\adac_span\span.c compiler\adac_errors\errors.c compiler\adac_lexer\lexer.c compiler\adac_ast\types.c compiler\adac_parse\parser.c compiler\adac_analysis\sema.c compiler\adac_codegen\x64.c compiler\adac_codegen\codegen.c compiler\adac_link\pe.c compiler\adac_driver\main.c build\library_data.c || exit /b 1

echo.
echo adac.exe built. Usage:  adac.exe examples\hello.adac -o hello.exe
