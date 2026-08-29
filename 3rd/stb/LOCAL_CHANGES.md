# stb (stb_image / stb_image_write) — 本地差异记录

- 上游：<https://github.com/nothings/stb>（单头库集合，本仓库仅取两枚：
  `stb_image.h` **v2.28**、`stb_image_write.h` **v1.16**，public domain）
- 接入方式：**header-only，单 TU 集中实现**。`Render/IRender.cpp` 中
  `#define STB_IMAGE_IMPLEMENTATION` / `STB_IMAGE_WRITE_IMPLEMENTATION`
  后包含本目录头文件。
- xmake：`target("stb")`，kind=phony（依赖关系显式化）。
- 路径历史：原位于 `DuiLib/Utils/`，2026-08 迁至本目录。

## 相对上游的已知改动

- 仅 UTF-8 BOM（上游 34cf1e3 批量转换）；未发现功能性改动。

## 将来评估

stb 头无版本宏约定，升级需人工核对文件头版本注释。换 xmake-repo 包
（`add_requires("stb")`）时注意包内头文件布局为 `stb/` 前缀包含
（`#include "stb/stb_image.h"`），IRender.cpp 包含路径需同步。
