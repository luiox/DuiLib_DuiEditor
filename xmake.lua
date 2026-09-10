-- DuiLib_DuiEditor 独立构建脚本（Windows / MSVC / GDI-GdiPlus 后端）
--
-- 说明：
-- * 本脚本只构建 DuiLib 核心库（Win32 后端），不含 DuiEditor / Demos / SDL 后端。
-- * Linux/macOS 的 SDL 后端构建走 DuiLib/CMakeLists.txt（上游维护）。
-- * 静态库使用方需自行定义 UILIB_STATIC（见 UIlib.h）。
-- * 排除项（remove_files）的完整清单、逐项理由与维护规则，
--   以 3rd/README.md「构建排除清单（单一事实源）」为唯一权威，勿在此处另立清单。
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

-- ATL 门控开关（默认全关 = 与既有行为一致；见 docs/KNOWN_ISSUES.md #20）
option("no_imageboxex")
    set_default(false)
    set_showmenu("define DUILIB_NO_IMAGEBOXEX: drop ATL(atlimage.h)-dependent CImageBoxExUI, no ATL needed")
option_end()
option("no_activex")
    set_default(false)
    set_showmenu("define DUILIB_NO_ACTIVEX: drop ATL-based CFlashUI/CWebBrowserUI (active-x controls)")
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
    -- ATL 门控（与 UIlib.h / ControlFactory.cpp 的宏一致；默认不启用）
    if has_config("no_imageboxex") then
        add_defines("DUILIB_NO_IMAGEBOXEX")
        remove_files("DuiLib/Control/UIImageBoxEx.cpp")
    end
    if has_config("no_activex") then
        add_defines("DUILIB_NO_ACTIVEX")
        remove_files("DuiLib/Control/UIFlash.cpp", "DuiLib/Control/UIWebBrowser.cpp")
    end

    if is_kind("shared") then
        -- advapi32/shell32：注册表（UIApplicationWin32）与托盘（TrayIcon）。
        -- 之前经 ATL 头的 defaultlib 指令隐式解析，ATL 门控构建下需显式声明。
        add_syslinks("user32", "gdi32", "comctl32", "ole32", "oleaut32", "imm32", "winmm", "version", "uxtheme", "advapi32", "shell32")
    end

-- 测试（KNOWN_ISSUES #18；零外部依赖的轻量断言，见 tests/test_main.cpp）
-- set_default(false)：不进默认构建；运行 xmake run duilib_test（或 xmake test）。
-- 仅 --kind=static（默认）下存在：nanosvg/stb 实现位于 IRender.cpp 单 TU 且
-- 不导出（无 UILIB_API），shared 配置下测试无法链接，属预期。
if is_config("kind", "static") then
target("duilib_test")
    set_kind("binary")
    set_default(false)
    set_languages("cxx17")
    add_deps("DuiLib", "pugixml")
    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
    else
        set_optimize("faster")
    end
    -- 与 DuiLib target 同口径的平台/字符集宏；UILIB_STATIC=消费静态库；
    -- PUGIXML_WCHAR_MODE 与 pugixml target 编译口径一致（测试 TU 只带声明）
    add_defines("WIN32", "_WIN32", "WINDOWS", "UNICODE", "_UNICODE",
        "UILIB_STATIC", "PUGIXML_WCHAR_MODE")
    -- ATL 门控与 DuiLib target 同步（工厂注册集合的行为面随门控变化）
    if has_config("no_imageboxex") then
        add_defines("DUILIB_NO_IMAGEBOXEX")
    end
    if has_config("no_activex") then
        add_defines("DUILIB_NO_ACTIVEX")
    end
    if is_arch("x64") then
        add_defines("_WIN64")
    end
    add_files("tests/*.cpp")
    add_syslinks("user32", "gdi32", "comctl32", "ole32", "oleaut32",
        "imm32", "winmm", "version", "uxtheme", "advapi32", "shell32")
    add_tests("default")
target_end()
end
