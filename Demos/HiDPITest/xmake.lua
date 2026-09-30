-- Demos/HiDPITest —— DPI 验证窗（KNOWN_ISSUES #22-#25 的验收工具）
--
-- 运行时切换 96~192 DPI，肉眼验证默认字体/固定尺寸/边框/文本内边距的
-- 整体缩放一致性；跨屏拖动验证 WM_DPICHANGED 链路。
-- 仅 Win32 后端（默认 GDI 引擎）；经根 xmake.lua includes 进同一工程，
-- set_default(false) 不进默认构建。
-- 用法：xmake build duilib_hidpi && xmake run duilib_hidpi

target("duilib_hidpi")
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
    -- 与 duilib_test 同口径的平台/字符集宏（UILIB_STATIC=消费静态库）
    add_defines("WIN32", "_WIN32", "WINDOWS", "UNICODE", "_UNICODE",
        "UILIB_STATIC", "PUGIXML_WCHAR_MODE")
    if is_arch("x64") then
        add_defines("_WIN64")
    end
    add_files("main.cpp")
    -- GUI 子程序：入口 _tWinMain（UNICODE 下展开为 wWinMain）
    add_ldflags("/SUBSYSTEM:WINDOWS", { force = true })
    add_syslinks("user32", "gdi32", "comctl32", "ole32", "oleaut32",
        "imm32", "winmm", "version", "uxtheme", "advapi32", "shell32")
target_end()
