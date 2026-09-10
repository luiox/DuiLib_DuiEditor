# stb (stb_image / stb_image_write) — 本地差异记录

- 上游：<https://github.com/nothings/stb>（单头库集合，本仓库仅取两枚：
  `stb_image.h` **v2.28**、`stb_image_write.h` **v1.16**，public domain）
- 接入方式：**header-only，单 TU 集中实现**。`Render/IRender.cpp` 中
  `#define STB_IMAGE_IMPLEMENTATION` / `STB_IMAGE_WRITE_IMPLEMENTATION`
  后包含本目录头文件。
- xmake：`target("stb")`，kind=phony（依赖关系显式化）。
- 路径历史：原位于 `DuiLib/Utils/`，2026-08 迁至本目录。

## 相对上游的已知改动

- **无功能性改动**：2026-09-11 以本地 clone 逐字节核对，`stb_image.h`
  v2.28 / `stb_image_write.h` v1.16 与 nothings/stb@`5736b15` 完全一致
  （仅 CRLF 行尾差异；该 commit 即 xrepo `stb` 包 2023.01.30 所钉）。
- 编码：**无 BOM**，CRLF 行尾（原"仅 UTF-8 BOM"记载有误，已更正）。

## 将来评估

**2026-09 核对结论：暂缓换包（裁决 c）**——内容与 xrepo `stb`
2023.01.30 完全一致，换包零内容风险；但会引入 configure 期网络依赖并
改变包含路径约定，与 mlaunch 经 submodule 源码编译的消费方式冲突，
留待集成后独立评估。换包时注意：
- 包安装头布局为 `stb/` 前缀（包同时注册了两种包含形式），
  `IRender.cpp` 的相对路径包含需同步；
- xrepo 后续新版本（>2023.01.30）会引入 v2.29+ 的解码行为变化，
  届时按 golden 测试（tests/）回归。
