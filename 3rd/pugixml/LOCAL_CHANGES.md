# pugixml — 本地差异记录

- 上游：<https://github.com/zeux/pugixml>（版本 1.11，`PUGIXML_VERSION 1110`）
- 接入方式：**header-only**。`DuiLib/StdAfx.h` 无条件 `#define PUGIXML_HEADER_ONLY`
  （`_UNICODE` 构建同时定义 `PUGIXML_WCHAR_MODE`），随后
  `#include "../3rd/pugixml/pugixml.hpp"`——pugi 实现以内联 COMDAT 进入每个
  含 StdAfx.h 的 TU。`pugixml.cpp` 因此**不参与编译**（无 HEADER_ONLY 宏的
  独立 TU 会产出强符号，shared 链接 LNK2005）。
- xmake：`target("pugixml")`，kind=headeronly（依赖关系显式化；包含为
  文件相对路径，无需导出 includedirs）。
- 路径历史：原位于 `DuiLib/Utils/pugixml/`，2026-08 迁至本目录。

## 相对上游 1.11 release 的已知改动

- 全部文件被本仓库上游批量转为 **UTF-8 with BOM**（仓库 34cf1e3
  「源码全部改为带BOM的UTF-8」），除此之外未发现功能性改动；如需精确
  diff 请对照 zeux/pugixml v1.11 tag。
- `ViewPugixml/`：VS 调试器可视化工具（上游自带，未使用，随库保留）。

## 将来评估

升级/换用 xmake-repo 标准包（`add_requires("pugixml")`）需先移除
StdAfx.h 的 HEADER_ONLY 接入并让 pugixml.cpp 独立成 TU——牵动全库编译
结构，待与上游合并策略一起定。
