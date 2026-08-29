# 3rd/ 第三方库登记表

> 目标（见 issue #2）：第三方库统一收口到本目录，与 DuiLib 核心源码解耦；
> 每库一目录 + `LOCAL_CHANGES.md` 差异记录，便于日后评估回归标准版本
> 或与上游（xfcanyue/DuiLib_DuiEditor）对照合并。
> 现阶段**不追求**换用 xmake-repo 标准包——魔改/接入方式先原样记录。

## 核心库构建实际依赖（根 xmake.lua 已注册 target）

| 目录 | 库 | 版本 | 接入方式 | target |
| --- | --- | --- | --- | --- |
| `pugixml/` | pugixml | 1.11 | header-only（StdAfx.h 全 TU 内联） | phony |
| `nanosvg/` | nanosvg + raster | 上游 2013-14 快照 | header-only（IRender.cpp 单 TU 实现） | phony |
| `stb/` | stb_image / stb_image_write | v2.28 / v1.16 | header-only（IRender.cpp 单 TU 实现） | phony |

三个 target 均为 `phony`（纯依赖登记）：包含走源码内文件相对路径，
target 的作用是把依赖关系显式化，为将来转编译 TU / 换标准包留接线位。

## 已在本目录、但核心库不依赖（应用层自取）

| 目录 | 库 | 使用方 | 说明 |
| --- | --- | --- | --- |
| `CxImage/` | CxImage | DuiEditor / Demos（VS 工程） | 核心 Render/Control/Utils 零引用；VS vcxproj 路径绑定，暂不动 |
| `scintilla/` | Scintilla | DuiEditor 编辑器（VS 工程） | 同上 |
| `SDL/` `SDL_gfx/` `SDL_image/` `SDL_ttf/` | SDL3 全家 | SDL 后端 / DuiPreviewer | Win32 构建整体排除；externals 由脚本拉取（`SDL编译说明.txt`） |

## 应用目录内嵌、暂不迁移

- `DuiScript/angelscript/` — AngelScript 脚本引擎，仅 DuiScript 插件 DLL
  使用（VS 工程路径绑定），与核心库无关。迁移应随 DuiScript 的现代化
  一起做，此处仅登记。

## 迁移历史

- 2026-08：pugixml / nanosvg / stb 自 `DuiLib/Utils/` 迁入（PR #3，
  issue #2）。此前 `3rd/SDL*`、`CxImage`、`scintilla` 为上游原有布局。
