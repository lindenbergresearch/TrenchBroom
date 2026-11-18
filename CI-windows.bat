@echo off
setlocal
set "TB_SOURCE_DIR=%~dp0."
if not defined TB_BUILD_DIR set "TB_BUILD_DIR=%TB_SOURCE_DIR%\cmakebuild"
if not defined QT6_INSTALL_DIR set "QT6_INSTALL_DIR=%QT_ROOT_DIR%"
if not defined QT6_INSTALL_DIR (
    echo Set QT6_INSTALL_DIR or QT_ROOT_DIR to the Qt 6.10 installation prefix.
    exit /b 1
)
cmake --version
cmake -S "%TB_SOURCE_DIR%" -B "%TB_BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT6_INSTALL_DIR%" -DCMAKE_BUILD_TYPE=Release -DTB_SUPPRESS_PCH=ON -DTB_BUILD_REGRESSION_TESTS=ON
if errorlevel 1 exit /b 1
cmake --build "%TB_BUILD_DIR%" --config Release --target TrenchBroom tb-regression-tests --parallel 4
if errorlevel 1 exit /b 1
ctest --test-dir "%TB_BUILD_DIR%" -C Release -R "^tb-regression-tests$" --output-on-failure
if errorlevel 1 exit /b 1
pushd "%TB_BUILD_DIR%"
cpack -C Release
if errorlevel 1 (
    popd
    exit /b 1
)
call generate_checksum.bat
if errorlevel 1 (
    popd
    exit /b 1
)
popd
endlocal
