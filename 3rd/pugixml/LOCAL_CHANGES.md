# pugixml — 本地差异记录

- 上游：<https://github.com/zeux/pugixml>（版本 1.11，`PUGIXML_VERSION 1110`）
- 接入方式：**独立编译单元**（2026-09 起）。根 `xmake.lua` 的
  `target("pugixml")`（kind=static）编译 `pugixml.cpp`；
  `DuiLib/StdAfx.h` 只 `#include "../3rd/pugixml/pugixml.hpp"` 引入声明，
  **不再**定义 `PUGIXML_HEADER_ONLY`。
- 宏一致性约束：
  - `_UNICODE` 构建下 pugi 头侧定义 `PUGIXML_WCHAR_MODE`（StdAfx.h 条件宏），
    pugixml target 因此无条件以 `PUGIXML_WCHAR_MODE` 编译——本仓 xmake 的
    DuiLib target 恒定 `UNICODE/_UNICODE`，两者天然一致。
  - 宿主若以非 UNICODE 方式自建 DuiLib 源码：`pugixml.cpp` 不要带该宏
    （与头侧保持 char 模式一致）。
  - 宿主若以 `DuiLib/**.cpp` glob 源码编译（如 mlaunch DuiLibLite），
    **必须同时编译 `3rd/pugixml/pugixml.cpp`**（或链接本 target），否则
    pugi 符号无法解析——此前 header-only 时代天然内联、无需此步。
- 路径历史：原位于 `DuiLib/Utils/pugixml/`，2026-08 迁至本目录。

## 相对上游 1.11 release 的已知改动

- 全部文件被本仓库上游批量转为 **UTF-8 with BOM**（仓库 34cf1e3
  「源码全部改为带BOM的UTF-8」），除此之外未发现功能性改动；如需精确
  diff 请对照 zeux/pugixml v1.11 tag。
- `ViewPugixml/`：VS 调试器可视化工具（上游自带，未使用，随库保留）。

## 上游合并策略（登记于 3rd/README.md「上游合并策略」节）

- 库文件零魔改（仅 BOM），升级/合并时 `pugixml.{cpp,hpp}`、`pugiconfig.hpp`
  可整体替换为上游对应版本，无需适配；
- fork 侧改动收敛在 **StdAfx.h 接入段**（去掉 HEADER_ONLY，约 5 行）与
  **根 xmake.lua 的 pugixml target**，两处即为与上游/宿主合并时的全部
  偏移点；上游 VS 工程（vcxproj）若要恢复可构建，需把
  `3rd/pugixml/pugixml.cpp` 加入编译项（本 fork 构建门为 xmake，不维护
  vcxproj）。

## 将来评估

已具备转 xmake-repo 标准包（`add_requires("pugixml")`）的编译结构前提
（独立 TU）；换包时需保证包版本同样以 `PUGIXML_WCHAR_MODE` 编译。
