@echo off
rem ============================================================================
rem  QR Studio build driver
rem
rem    build.bat            configure + build            (same as: build.bat build)
rem    build.bat check      validate the toolchain only
rem    build.bat test       build, then run Rust + C++ + Python tests
rem    build.bat deploy     build, then copy the Qt runtime next to the app (dist\)
rem    build.bat clean      delete the build\ and dist\ folders
rem    build.bat all        build + test + deploy
rem
rem  Toolchain locations are the ones verified on this machine. Override any of
rem  them by setting the variable before calling this script:
rem    QRS_QT_DIR, QRS_MINGW_DIR, QRS_CMAKE_DIR, QRS_NINJA_DIR, QRS_PYTHON
rem ============================================================================
setlocal EnableExtensions

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "DIST_DIR=%ROOT%\dist"

rem Tool installs live under deps\ (git-ignored); see docs\build.md.
set "DEPS_DIR=%ROOT%\deps"
if not defined QRS_QT_DIR    set "QRS_QT_DIR=%DEPS_DIR%\qt6\6.12.0\mingw_64"
if not defined QRS_MINGW_DIR set "QRS_MINGW_DIR=%DEPS_DIR%\qt6\Tools\mingw1310_64"
if not defined QRS_CMAKE_DIR set "QRS_CMAKE_DIR=%DEPS_DIR%\qt6\Tools\CMake_64\bin"
if not defined QRS_NINJA_DIR set "QRS_NINJA_DIR=%DEPS_DIR%\qt6\Tools\Ninja"
if not defined QRS_PYTHON    set "QRS_PYTHON=python"

set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=build"

if /i "%ACTION%"=="clean" goto :clean

rem cargo is looked up on the caller's PATH before the Qt toolchain is prepended.
set "CARGO_EXE="
for /f "delims=" %%I in ('where cargo 2^>nul') do if not defined CARGO_EXE set "CARGO_EXE=%%I"

set "PATH=%QRS_MINGW_DIR%\bin;%QRS_QT_DIR%\bin;%QRS_CMAKE_DIR%;%QRS_NINJA_DIR%;%PATH%"

call :check || exit /b 1
if /i "%ACTION%"=="check" exit /b 0

if /i "%ACTION%"=="build"  ( call :build  || exit /b 1 & exit /b 0 )
if /i "%ACTION%"=="test"   ( call :build  || exit /b 1 & call :test || exit /b 1 & exit /b 0 )
if /i "%ACTION%"=="deploy" ( call :build  || exit /b 1 & call :deploy || exit /b 1 & exit /b 0 )
if /i "%ACTION%"=="all"    ( call :build  || exit /b 1 & call :test || exit /b 1 & call :deploy || exit /b 1 & exit /b 0 )

echo [build] Unknown action "%ACTION%". Use: check ^| build ^| test ^| deploy ^| clean ^| all
exit /b 2

rem ----------------------------------------------------------------------------
:check
set "MISSING="
call :need "%QRS_MINGW_DIR%\bin\g++.exe"         "MinGW g++ (QRS_MINGW_DIR)"
call :need "%QRS_QT_DIR%\bin\qmake.exe"          "Qt 6 MinGW kit (QRS_QT_DIR)"
call :need "%QRS_QT_DIR%\bin\windeployqt.exe"    "windeployqt (QRS_QT_DIR)"
call :need "%QRS_CMAKE_DIR%\cmake.exe"           "CMake (QRS_CMAKE_DIR)"
call :need "%QRS_NINJA_DIR%\ninja.exe"           "Ninja (QRS_NINJA_DIR)"
if not defined CARGO_EXE (
    echo [check] MISSING  cargo ^(Rust^) - not found on PATH
    set "MISSING=1"
) else (
    echo [check] ok       cargo: %CARGO_EXE%
)
"%QRS_PYTHON%" --version >nul 2>&1
if errorlevel 1 (
    echo [check] warning  Python not found ^(QRS_PYTHON^) - Python tests will be skipped
    set "HAVE_PYTHON="
) else (
    echo [check] ok       python: %QRS_PYTHON%
    set "HAVE_PYTHON=1"
)
if defined MISSING (
    echo [check] Toolchain incomplete. See docs\build.md.
    exit /b 1
)
exit /b 0

:need
rem No parenthesised blocks here: the labels themselves contain parentheses.
if exist "%~1" echo [check] ok       %~2
if exist "%~1" exit /b 0
echo [check] MISSING  %~2: %~1
set "MISSING=1"
exit /b 0

rem ----------------------------------------------------------------------------
:build
rem Always configure: it takes about a second when nothing changed and picks
rem up newly added source folders without a manual clean.
echo [build] Configuring...
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    "-DCMAKE_PREFIX_PATH=%QRS_QT_DIR%" ^
    "-DCMAKE_C_COMPILER=%QRS_MINGW_DIR%\bin\gcc.exe" ^
    "-DCMAKE_CXX_COMPILER=%QRS_MINGW_DIR%\bin\g++.exe" ^
    "-DCARGO_EXECUTABLE=%CARGO_EXE%" > "%BUILD_DIR%.configure.log" 2>&1
if errorlevel 1 (
    type "%BUILD_DIR%.configure.log"
    echo [build] Configure FAILED
    exit /b 1
)
del "%BUILD_DIR%.configure.log" >nul 2>&1
echo [build] Compiling...
cmake --build "%BUILD_DIR%"
if errorlevel 1 ( echo [build] Build FAILED & exit /b 1 )
echo [build] OK - binaries in %BUILD_DIR%\bin
exit /b 0

rem ----------------------------------------------------------------------------
:test
set "FAILED="
echo [test] Rust unit tests...
for %%D in ("%CARGO_EXE%") do set "CARGO_DIR=%%~dpD"
set "CARGO_TARGET_DIR=%BUILD_DIR%\rust"
if exist "%CARGO_DIR%gcc.exe" set "CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=%CARGO_DIR%gcc.exe"
"%CARGO_EXE%" test --offline --quiet --manifest-path "%ROOT%\rust\qrs_encode\Cargo.toml"
if errorlevel 1 set "FAILED=%FAILED% rust"

echo [test] C++ tests ^(ctest^)...
set "QT_QPA_PLATFORM=offscreen"
ctest --test-dir "%BUILD_DIR%" --output-on-failure
if errorlevel 1 set "FAILED=%FAILED% cpp"
set "QT_QPA_PLATFORM="

if defined HAVE_PYTHON (
    echo [test] Python interop tests ^(pytest^)...
    "%QRS_PYTHON%" -m pytest "%ROOT%\tests\python" -q -p no:cacheprovider
    if errorlevel 1 set "FAILED=%FAILED% python"
) else (
    echo [test] SKIPPED Python tests - no interpreter
)

if defined FAILED (
    echo [test] FAILED:%FAILED%
    exit /b 1
)
echo [test] All test suites passed
exit /b 0

rem ----------------------------------------------------------------------------
:deploy
if not exist "%BUILD_DIR%\bin\QRStudio.exe" (
    echo [deploy] QRStudio.exe has not been built
    exit /b 1
)
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
copy /y "%BUILD_DIR%\bin\QRStudio.exe"   "%DIST_DIR%\" >nul
copy /y "%BUILD_DIR%\bin\qrs_cli.exe"    "%DIST_DIR%\" >nul
copy /y "%BUILD_DIR%\bin\qrs_encode.dll" "%DIST_DIR%\" >nul
copy /y "%ROOT%\tools\windows\context_menu.ps1" "%DIST_DIR%\" >nul
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw --compiler-runtime "%DIST_DIR%\QRStudio.exe"
if errorlevel 1 ( echo [deploy] windeployqt FAILED & exit /b 1 )
echo [deploy] OK - self-contained app in %DIST_DIR%
exit /b 0

rem ----------------------------------------------------------------------------
:clean
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%DIST_DIR%"  rmdir /s /q "%DIST_DIR%"
echo [clean] Removed build\ and dist\
exit /b 0
