@echo off
echo ============================================
echo  Building A46 Computer Emulator
echo ============================================

:: Try g++ first (MinGW)
where g++ >nul 2>nul
if %ERRORLEVEL% == 0 (
    echo [*] Using g++ (MinGW)
    g++ -std=c++17 -O2 -o a46.exe main.cpp -lgdi32 -luser32 -lcomctl32 -lcomdlg32 -mwindows
    if %ERRORLEVEL% == 0 (
        echo [OK] Built a46.exe successfully!
        echo [*] Run: a46.exe
    ) else (
        echo [FAIL] Build failed.
    )
    goto :eof
)

:: Try cl.exe (MSVC)
where cl >nul 2>nul
if %ERRORLEVEL% == 0 (
    echo [*] Using cl.exe (MSVC)
    cl /EHsc /O2 /Fe:a46.exe main.cpp user32.lib gdi32.lib comctl32.lib comdlg32.lib /link /SUBSYSTEM:WINDOWS
    if %ERRORLEVEL% == 0 (
        echo [OK] Built a46.exe successfully!
        echo [*] Run: a46.exe
    ) else (
        echo [FAIL] Build failed.
    )
    goto :eof
)

echo [ERROR] No C++ compiler found!
echo Install MinGW-w64 (g++) or Visual Studio (cl.exe)
echo   MinGW: https://www.mingw-w64.org/
echo   VS:    https://visualstudio.microsoft.com/
