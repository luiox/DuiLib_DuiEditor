# nanosvg — 本地差异记录

- 上游：<https://github.com/memononen/nanosvg>（memononen 版，
  `Copyright 2013-14 Mikko Mononen`，无版本号发布；raster 模块含
  Anti-Grain Geometry 2.4 的光栅化派生代码）
- 接入方式：**header-only，单 TU 集中实现**。`Render/IRender.cpp` 中
  `#define NANOSVG_IMPLEMENTATION` / `NANOSVGRAST_IMPLEMENTATION` 后包含
  本目录头文件（mlaunch 的 SVG 图标渲染依赖此路径）。
- xmake：`target("nanosvg")`，kind=phony（依赖关系显式化）。
- 路径历史：原位于 `DuiLib/Utils/`，2026-08 迁至本目录。

## 相对上游的已知改动

- **memcpy 边界检查修复**：源自上游 DuiLib 仓库社区提交
  （`fix: add bounds check before memcpy in nanosvg.h`，见上游仓库
  PR #11 / V-001 安全修复），本 fork 继承。
- 全部文件带 UTF-8 BOM（上游 34cf1e3 批量转换）。

## 将来评估

nanosvg 上游久未活跃更新；如换用 xmake-repo 包（`add_requires("nanosvg")`）
需验证 memcpy 修复是否已含于所选版本，以及 IRender.cpp 的
IMPLEMENTATION 宏集中方式与包安装头是否兼容。
