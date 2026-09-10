# 3rd/ 第三方库登记表

> 目标（见 issue #2）：第三方库统一收口到本目录，与 DuiLib 核心源码解耦；
> 每库一目录 + `LOCAL_CHANGES.md` 差异记录，便于日后评估回归标准版本
> 或与上游（xfcanyue/DuiLib_DuiEditor）对照合并。
> 现阶段**不追求**换用 xmake-repo 标准包——魔改/接入方式先原样记录。

## 核心库构建实际依赖（根 xmake.lua 已注册 target）

| 目录 | 库 | 版本 | 接入方式 | target |
| --- | --- | --- | --- | --- |
| `pugixml/` | pugixml | 1.11 | **独立编译单元**：`pugixml.cpp` 由本 target 编译（`PUGIXML_WCHAR_MODE`），`StdAfx.h` 只 include 头 | static |
| `nanosvg/` | nanosvg + raster | 上游 2013-14 快照 | header-only（IRender.cpp 单 TU 实现） | phony |
| `stb/` | stb_image / stb_image_write | v2.28 / v1.16 | header-only（IRender.cpp 单 TU 实现） | phony |

pugixml 自 2026-09 起为独立 TU（`DuiLib/**.cpp` glob 不含它，实现不进
DuiLib 库文件）；nanosvg/stb 仍为 `phony`（纯依赖登记）：包含走源码内
文件相对路径，target 的作用是把依赖关系显式化，为将来转编译 TU / 换
标准包留接线位。

## 构建门控（2026-09，含文档更正登记）

核心库支持无 ATL 组件构建（详见 `docs/KNOWN_ISSUES.md` #20）：

- `DUILIB_NO_IMAGEBOXEX`：剔除 `CImageBoxExUI`（atlimage.h 依赖）；
- `DUILIB_NO_ACTIVEX`：剔除 `CFlashUI` / `CWebBrowserUI`（atlconv/atlcomcli 依赖）。

**文档更正**：原 04 号架构文档/KNOWN_ISSUES #20 备注认为仅
`DUILIB_NO_IMAGEBOXEX` 即可无 ATL 全量构建——实测不成立（Flash/WebBrowser
同样依赖 ATL，且 ImageBoxEx 门控暴露 UIMsgWnd/UIFile/UIApplicationWin32
等处的隐藏传递依赖），已按实际情况一并落地。xmake 开关
`--no_imageboxex=y` / `--no_activex=y`（默认全关，行为与既往一致）。

## 上游合并策略（各库一节，随差异登记更新）

| 库 | 库文件魔改 | fork 侧偏移点（与上游/宿主合并时的全部触点） |
| --- | --- | --- |
| `pugixml/` | **零魔改**（仅 UTF-8 BOM，见 `pugixml/LOCAL_CHANGES.md`） | ① `DuiLib/StdAfx.h` 接入段（去 `PUGIXML_HEADER_ONLY`，约 5 行）；② 根 `xmake.lua` `target("pugixml")`（static + `PUGIXML_WCHAR_MODE`）。库文件可整体替换为上游版本 |
| `nanosvg/` | **memcpy 边界检查修复**（上游 DuiLib 仓库 PR#11 继承，见 `nanosvg/LOCAL_CHANGES.md`） | `Render/IRender.cpp` 的 `NANOSVG*_IMPLEMENTATION` 单 TU 接入段 |
| `stb/` | 零魔改（仅 UTF-8 BOM） | 同上（`STB_IMAGE*_IMPLEMENTATION` 单 TU 接入段） |

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
- 2026-09：pugixml 由 header-only（StdAfx.h 全 TU 内联）转为独立编译
  单元（本目录 `pugixml.cpp` 经根 `xmake.lua` target 编译）。**宿主以
  glob 方式编 DuiLib 源码的，必须同步编译 `3rd/pugixml/pugixml.cpp`**，
  详见 `pugixml/LOCAL_CHANGES.md`。
