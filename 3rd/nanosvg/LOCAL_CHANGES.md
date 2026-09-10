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
  PR #11 / V-001 安全修复），本 fork 继承。经 git 逐字节核实，这是
  仓内副本相对继承快照的**唯一**改动。
- 编码：**无 BOM**，CRLF 行尾（2026-09 核实，原"仅 UTF-8 BOM"记载有误，
  已更正）。

## 将来评估

nanosvg 上游久未活跃更新。**2026-09 核对结论：保留 vendored（裁决 b）**：
上游 memononen/nanosvg master 与 xrepo `nanosvg` 包 2022.07.09
（commit 7e37e00）的 `nsvg__createGradient` 均无 memcpy bounds check
（2026-09-11 抓取核对），换包=回归安全修复。证据详见 3rd/README.md
「xrepo 化核对结论」。
