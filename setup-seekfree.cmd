@echo off
REM ============================================================================
REM  setup-seekfree.cmd - 为本工程重建逐飞开源库的 4 个目录联接(junction)
REM
REM  为什么需要这一步：
REM    逐飞库体积不小且属于第三方 GPL-3.0 代码，本工程不把它放进仓库，
REM    而是通过工程根下的 4 个 junction 引用一份**共享副本**：
REM        zf_common / zf_driver / zf_device / zf_components
REM    junction 无法被 git 保存（会被当普通目录展开），所以克隆后要跑一次本脚本。
REM
REM  用法：
REM     setup-seekfree.cmd                  :: 用默认位置 E:\RISC-V\libraries\CHV307_Library
REM     setup-seekfree.cmd D:\libs\CHV307_Library
REM
REM  可重复执行（幂等）。
REM ============================================================================
setlocal enabledelayedexpansion

set "PROJ=%~dp0"
if "%PROJ:~-1%"=="\" set "PROJ=%PROJ:~0,-1%"

set "LIBDIR=%~1"
if "%LIBDIR%"=="" set "LIBDIR=E:\RISC-V\libraries\CHV307_Library"
set "LIBSRC=%LIBDIR%\Seekfree_CH32V307VCT6_Opensource_Library\libraries"

echo [INFO] 工程   : %PROJ%
echo [INFO] 共享库 : %LIBDIR%
echo.

REM ---------------------------------------------------------------- 1) 库是否存在
if not exist "%LIBSRC%\zf_common\zf_common_headfile.h" (
    echo [WARN] 没找到逐飞库，准备克隆 ^(浅克隆，约 21MB^)
    if not exist "%LIBDIR%" mkdir "%LIBDIR%"
    REM 全局若配了不可用的代理，这里同时禁用代理并强制 openssl 后端
    git -c http.sslBackend=openssl -c http.proxy= -c https.proxy= clone --depth 1 ^
        https://gitee.com/seekfree/CHV307_Library.git "%LIBDIR%"
    if errorlevel 1 (
        echo.
        echo [ERR] 克隆失败。请手动下载逐飞 CH32V307 开源库到：
        echo       %LIBDIR%
        echo       来源: https://gitee.com/seekfree/CHV307_Library
        exit /b 1
    )
    REM 让以后 git pull 也走直连 + openssl
    pushd "%LIBDIR%"
    git config --local http.proxy ""
    git config --local https.proxy ""
    git config --local http.sslBackend openssl
    popd
    echo [OK] 克隆完成
)

if not exist "%LIBSRC%\zf_common\zf_common_headfile.h" (
    echo [ERR] 库结构不对，缺少 zf_common\zf_common_headfile.h
    exit /b 1
)

REM ---------------------------------------------------------------- 2) 建 junction
for %%Z in (zf_common zf_driver zf_device zf_components) do (
    if exist "%PROJ%\%%Z" (
        echo [SKIP] %%Z 已存在
    ) else (
        if not exist "%LIBSRC%\%%Z" (
            echo [ERR] 库中缺少目录: %LIBSRC%\%%Z
            exit /b 1
        )
        mklink /J "%PROJ%\%%Z" "%LIBSRC%\%%Z" >nul
        if errorlevel 1 (
            echo [ERR] 建立 junction 失败: %%Z
            exit /b 1
        )
        echo [OK] %%Z  --^>  %LIBSRC%\%%Z
    )
)

echo.
echo ================================ 完成 ================================
echo 现在可以用 MounRiver Studio 打开本工程，然后 Project -^> Clean -^> Build
echo.
echo 注意：调试串口是 UART3 的 PB10(TX)/PB11(RX) @115200，不是 PA9。
echo ======================================================================
endlocal
