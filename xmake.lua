-- DuiLib_DuiEditor 独立构建脚本（Windows / MSVC / GDI-GdiPlus 后端）
--
-- 说明：
-- * 本脚本只构建 DuiLib 核心库（Win32 后端），不含 DuiEditor / Demos / SDL 后端。
-- * Linux/macOS 的 SDL 后端构建走 DuiLib/CMakeLists.txt（上游维护）。
-- * 静态库使用方需自行定义 UILIB_STATIC（见 UIlib.h）。
-- * 排除项说明：
--   - *Sdl*.cpp：SDL 后端源码，其头文件在 #ifdef DUILIB_SDL 之前无条件包含 SDL 头，
--     非 SDL 构建必须整组排除（后端由 DUILIB_SDL 宏门控，Win32 路径不定义该宏）。
--   - Utils/unzip.cpp：依赖 zlib 的解压工具路径，宿主按需自带。
--   - Utils/UIDataExchange.cpp：依赖旧式数据绑定组件，宿主按需自带。
--
-- * 第三方库：位于 3rd/（见 3rd/README.md 登记表）。pugixml 为独立编译
--   单元（static target）；nanosvg / stb 仍为 header-only——*_IMPLEMENTATION
--   集中在 Render/IRender.cpp 单 TU，phony target 仅登记依赖关系。

set_project("DuiLib_DuiEditor")
set_version("1.1.0")
set_xmakever("2.8.3")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "." })

option("kind")
    set_default("static")
    set_showmenu("target kind: static(default) or shared")
option_end()

-- 第三方库 target（用法与差异见各目录 LOCAL_CHANGES.md；排除纪律以 3rd/README.md 为单一事实源）
-- pugixml：独立编译单元（pugixml.cpp 由此 target 编译，DuiLib/StdAfx.h 只 include 头）。
--   PUGIXML_WCHAR_MODE 必须与 StdAfx.h 的 _UNICODE 条件宏一致——本仓 DuiLib target
--   恒定 UNICODE/_UNICODE，故此处无条件定义。
target("pugixml")
    set_kind("static")
    add_files("3rd/pugixml/pugixml.cpp")
    add_defines("PUGIXML_WCHAR_MODE")
    add_headerfiles("3rd/pugixml/*.hpp")
target_end()

target("nanosvg")
    set_kind("phony")
    add_headerfiles("3rd/nanosvg/*.h")
target_end()

target("stb")
    set_kind("phony")
    add_headerfiles("3rd/stb/*.h")
target_end()

target("DuiLib")
    set_kind("$(kind)")
    set_languages("cxx17")
    add_deps("pugixml", "nanosvg", "stb")

    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
    else
        set_optimize("faster")
        set_strip("all")
    end

    add_defines("WIN32", "_WIN32", "WINDOWS")
    if is_arch("x64") then
        add_defines("_WIN64")
    end
    add_defines("UNICODE", "_UNICODE")
    -- UIlib.h 依据该宏选择 Win32/SDL 实现头；Win32 构建不定义 DUILIB_SDL。
    add_defines("UILIB_EXPORTS")
    if is_kind("static") then
        add_defines("UILIB_STATIC")
    end

    add_includedirs("DuiLib", { public = true })
    add_files("DuiLib/**.cpp")
    remove_files(
        "DuiLib/**/*Sdl.cpp",
        "DuiLib/**/*SDL.cpp",
        "DuiLib/Utils/unzip.cpp",
        "DuiLib/Utils/UIDataExchange.cpp"
    )

    if is_kind("shared") then
        add_syslinks("user32", "gdi32", "comctl32", "ole32", "oleaut32", "imm32", "winmm", "version", "uxtheme")
    end
