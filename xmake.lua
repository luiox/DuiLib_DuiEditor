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

set_project("DuiLib_DuiEditor")
set_version("1.1.0")
set_xmakever("2.8.3")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "." })

option("kind")
    set_default("static")
    set_showmenu("target kind: static(default) or shared")
option_end()

target("DuiLib")
    set_kind("$(kind)")
    set_languages("cxx17")

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
