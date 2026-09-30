# DuiLib（本 fork）已知问题清单

> 来源：mlaunch（C++/DuiLib 启动器）重写过程中的实测踩坑，逐条在代码层核实过。
> 状态标记：[已修] 本 fork 已修复；[待修] 计划在本 fork 修复；[规避] 宿主侧绕过，库内暂不动。
> 修复原则：每个修复保持最小 diff、附实测场景说明，便于日后与上游（xfcanyue/DuiLib_DuiEditor）对照合并。

## 渲染 / 绘制

### 1. 全量重排后局部 update region 留下陈旧像素 [已修]
- 位置：`DuiLib/Core/UIManagerWin32.cpp` `CPaintManagerWin32UI::OnPaint`
- 现象：`m_pRoot->IsUpdateNeeded()` 分支重排后仍按旧 update region 绘制，
  被移动/显隐的子控件区域出现残影（实测：搜索模式切换后 body 未下移的"鬼影"）。
- 修复：重排分支末尾 `rcPaint = rcClient` 强制整窗重绘。

### 2. LockUpdate 期间仍实际执行绘制，DWM 显示旧帧被拉伸 [已修，2026-09-09 修订]
- 位置：同文件 `OnPaint`
- 现象：宿主在 WM_ENTERSIZEMOVE/WM_SIZING 期间 `LockUpdate(true)` 想去闪烁，
  但绘制路径照走，DWM 拉伸旧帧反而闪烁。
- 修复：`m_bLockUpdate` 时不重排、不重绘控件（`m_bUpdateNeeded` 重排分支同样被
  锁定挡住），WM_EXITSIZEMOVE 后由宿主统一重排。
- 修订（2026-09-09，见 #21）：最初实现为"仅 BeginPaint/EndPaint 校验、完全不绘制"，
  副作用是窗口扩大时新暴露条带被校验后永不绘制（空白/未定义像素，即 mlaunch
  "向外拖大时新区域不显示"）。现锁定期仅对新暴露条带补底送显，不重绘控件、
  不重排的去闪烁策略不变。

### 3. `CControlUI::NeedUpdate` 对不可见控件直接早退 [已修 2026-09]
- 位置：`DuiLib/Core/UIControl.cpp` `NeedUpdate`
- 现象：`NeedUpdate()` 开头 `if(!IsVisible()) return;`——控件在隐藏状态下被
  标记更新会被丢弃，之后 `SetVisible(true)` 若无显式 NeedUpdate 不会重绘。
  宿主被迫在显隐切换处手工 `GetRoot()->NeedUpdate()`。
- 修复：不可见时仍置 `m_bUpdateNeeded = true`，仅跳过 Invalidate 与
  manager 级通知；显示时经 `SetVisible→NeedParentUpdate` 链触发重绘，
  置位标记由 OnPaint 的 update 分支兑现。
- 回归门：`test_needupdate_while_hidden`（隐藏期置位断言，修复前为 false）。

### 4. 部分更新（partial update）路径依赖陈旧 m_rcItem [规避]
- 位置：`OnPaint` 的 `__FindControlsFromUpdate` 单控件分支
- 现象：只对"被标记控件"按其当前 m_rcItem 重绘，控件自身 rect 变了但父容器
  未重排时画错位置。宿主侧统一走整窗重排规避（见 mlaunch UpdateSearchUi）。

### 21. 窗口缩放三症状：扩大方向新区域空白 / 释放鼠标瞬间卡顿 / 后备位图反复销毁重建 [已修 2026-09-09]
- 位置：`DuiLib/Core/UIManagerWin32.cpp` `OnPaint` 锁分支、
  `DuiLib/Render/UIRender_gdi.cpp` `Resize`、`DuiLib/Core/UIWindowWin32.cpp:19`
  （窗口类仅 CS_DBLCLKS，无 CS_HREDRAW/CS_VREDRAW）。
- 根因：
  ① 无 CS_HREDRAW/CS_VREDRAW 时系统对"扩大"只无效化新暴露条带；锁分支
    `BeginPaint` 把它校验掉却从不绘制，且该校验发生在 `Render()->Resize`
    之前——后备位图仍是旧尺寸，条带直到释放鼠标都无有效像素（症状一）。
  ② `Resize` 每次尺寸变化销毁+重建 DIB（`CreateDIBSection` 全零初始化=黑图），
    释放鼠标那一帧把"全树 SetPos 重排 + 大位图重分配 + 整窗 ClearAlpha/重绘/
    BitBlt + 阴影 Update"压缩进一帧（症状二/三的释放爆发部分）。
- 修复（最小 diff；**不**加 CS_HREDRAW/CS_VREDRAW——那会每个 sizing tick
  全窗无效化，重绘量反而剧增）：
  ① `UIRender_gdi::Resize` 改为"新建位图→旧内容左上对齐逐行拷入→SelectObject
    切换并释放旧位图"；同尺寸调用维持早退。释放鼠标帧的 Resize 命中同尺寸
    早退，大块分配从释放瞬间消失。
  ② `OnPaint` 锁分支：仍不重排、不重绘控件（c329337 去闪烁策略保持），
    但对"旧后备位图范围之外"的新暴露条带（右侧/底部两块，互斥不重叠）按
    `GetRoot()->GetBkColor()` 补底（未设置时兜底不透明黑）并随帧 BitBlt 送显；
    旧区域内的偶发局部无效化不重画，避免拖拽中把已显示内容抹成背景色。
- 适用范围：内容保留在 GDI 后端实现（mlaunch 实际路径）；gdiplus/SDL 后端
  `Resize` 未改，锁分支的条带补底逻辑为后端无关，仅"旧区像素保留"在那些
  后端缺失（锁定期 ps.rcPaint 通常只含新条带，实际影响可忽略）。
- 实测场景：mlaunch 主窗（12 组 171 条列表）向外/向内拖拽缩放：扩大过程新
  区域即时显示窗口背景色，无空白/黑块/残影；释放后一帧完成重排，无可感知停顿。
- 遗留：锁定期内容仍为"旧布局锚定左上 + 新区背景色"，重排只在结束后发生
  （对齐原版行为）；持续缩放中的控件级重绘去闪烁（WM_SYNCPAINT/脏区合并）
  仍为后续方向。

## 输入 / 焦点

### 5. 原生 EDIT 固色画刷每次 WM_SIZE 重建（浪费），黑块根因未定位 [待修]
- 位置：`DuiLib/Control/UIEditWndWin32.cpp` `HandleMessage` WM_SIZE 分支
- 澄清：曾把"EDIT 背景黑块"归因于固色路径每次 WM_SIZE 销毁/重建 solid brush
  （"EDIT 缓存 CTLCOLOREDIT 旧句柄、下次绘制用到已删除句柄"），并曾以
  "仅首次创建"修复。复查确认该机制不成立——WM_CTLCOLOREDIT 每次 erase 由宿主
  **同步**询问、同步使用，原生 EDIT 不跨绘制缓存返回的句柄，不存在"用已删除
  句柄"链路；且"仅首次创建"会让运行时 `SetNativeEditBkColor` 的换色永不生效。
  已回退为上游行为，留此记录防止再次误诊。
- 黑块候选根因（未隔离）：① 位图路径（`clrColor < 0xFF000000`）
  `CreateControlBackBitmap` 快照了未渲染区域；② `0xFFFFFFFF` 哨兵值路径
  （CTLCOLOREDIT 早退后 EDIT 用默认擦除）。mlaunch 实测时改为完全不设置
  nativebkcolor 规避，暂无稳定复现场景。
- 待办：固色路径"每次重建"属浪费可优化，但需先复现黑块、定位真实根因后一并
  处理两条路径，避免按错误机制打补丁。

### 6. 原生 EDIT 内按键宿主收不到（ESC 等） [非库问题]
- 位置：`UIEditWndWin32.cpp` WM_KEYDOWN 链（Win32 机制：子窗口键盘消息只到子窗口）
- 现象：EDIT 持有焦点时 WM_KEYDOWN 不经过宿主窗口过程，宿主无法直接响应
  特定键位（如"ESC 退出搜索模式"这类**宿主业务交互**）。
- 定性：**不是库 bug**。单行 EDIT 的 ESC 在 Win32 下本就无默认行为，谈不上
  "被吞"；把具体键位映射成业务动作属于宿主职责，不应改库（曾误将
  `SendNotify(owner, _T("escape"))` 回迁入库，已回退——私有事件名进库会
  成为与上游合并的偏移点，也会给其他宿主引入未知通知）。
- 宿主解法：实现 `ITranslateAccelerator` 并 `AddTranslateAccelerator`。
  fork 的 `CPaintManagerUI::TranslateMessage`（UIManager.cpp，消息循环
  DispatchMessage 之前）对子窗口消息也会先调它，可拦截发往原生 EDIT 的
  键盘消息。返回值约定看聚合器 `CPaintManagerUI::TranslateAccelerator`：
  `lResult == S_OK` 才吞掉（返回 true → 消息不派发），S_FALSE/其他=放行。
  注意该回调对经过消息循环的**所有**消息（含鼠标）都会触发，放行分支必须
  返回 S_FALSE，误返回 S_OK 会吞掉整窗输入。mlaunch AppWindow（搜索框
  ESC）与 SettingsWindow（捕获态 TAB）均此方案。
- 对比：同文件的 `VK_RETURN → SendNotify(DUI_MSGTYPE_RETURN)` 与
  layerd 分支的 `VK_TAB → SetNextTabControl` 是**上游基线既有行为**，不动。

### 7. 原生 EditWnd 失焦即异步自毁（PostMessage WM_CLOSE） [待修]
- 位置：`UIEditWndWin32.cpp` `OnKillFocus`
- 现象：切换输入框时旧 EDIT 异步残留，FindWindowEx 命中错误窗口、新控件
  SETFOCUS 不触发重建。宿主需先同步 WM_CLOSE 销毁旧 EDIT 再聚焦新控件
  （见宿主 edit_focus_helper）。
- 建议方案：OnKillFocus 改同步 DestroyWindow 或提供宿主可调的同步销毁入口。

### 8. `CPaintManagerUI::SetFocus` 对已焦点控件早退，SETFOCUS 事件丢失 [待修]
- 位置：`DuiLib/Core/UIManager.cpp:1040`
- 现象：`pControl == m_pFocus` 直接 return；窗口重激活后程序化聚焦同一
  EDIT 时 SETFOCUS 不发、原生 EDIT 不重建。宿主需"清焦点→设回"或模拟点击。

### 9. `CEditUI::DoEvent` 无 UIEVENT_CHAR 分支 [待修]
- 位置：`DuiLib/Control/UIEdit.cpp:59`
- 现象：原生 EDIT 不存在时（未创建/已自毁），字符输入被整体丢弃。
  宿主需把 WM_CHAR 转发给原生 EDIT 或走 BUTTONDOWN 模拟路径。

### 10. `PreMessageHandler` 抢先处理 VK_TAB（消息循环层） [规避]
- 位置：`DuiLib/Core/UIManager.cpp` `CPaintManagerUI::MessageLoop` / `PreMessageHandler`
- 现象：TAB 在**消息循环派发前**就被 `PreMessageHandler` 拦截执行
  `SetNextTabControl`（早于宿主窗口过程，也早于 m_pm.MessageHandler），
  宿主既收不到 TAB 也读不到可信的 m_pFocus，Tab 轮换需自管索引。
- 解法：宿主实现 fork 的 `ITranslateAccelerator` 并
  `AddTranslateAccelerator`——这是唯一早于 PreMessageHandler 的扩展点
  （mlaunch SettingsWindow 实测）。

### 11. Shift+F10 被吞，不产生 WM_CONTEXTMENU [待修]
- 现象：键盘右键菜单键路径失效。宿主用 Ctrl+M / Apps 键 + 手工补发
  WM_CONTEXTMENU 规避。

## 控件属性 / API

### 12. CButtonUI 不支持 normalbkcolor/hotbkcolor/pushedbkcolor [误诊更正 + 别名 2026-09]
- 原登记（按 `UIButton.cpp:247` SetAttribute 判断三态底色被静默忽略）经复查
  **不成立**：`hotbkcolor`/`pushedbkcolor` 在 `CControlUI::SetAttribute`
  （UIControl.cpp）本就解析为 `SetHotBkColor/SetPushedBkColor`，且
  `PaintBkColor` 有完整的 disabled→select→pushed→hot→focus→bkcolor 状态
  回退链——按钮三态底色一直可用。
- 真实缺口仅 `normalbkcolor` 这个属性名不存在（normal 态底色本库语义即
  `bkcolor`），写了会被静默忽略、极易误用。
- 修复：`CControlUI::SetAttribute` 增加 `normalbkcolor` → `SetBkColor`
  别名（与既有 `bkcolor1` 别名同法）。
- 回归门：`test_button_state_bkcolor_attrs`（三属性解析断言）。

### 13. CCheckBoxUI 视觉态依赖图片资源 [待修]
- 现象：无 selectedstateimage 等资源时复选框无可见勾选态。
- 建议方案：无图片时回退自绘（边框 + 对勾），与 #12 一并处理。

### 14. `WindowImplBase::MessageHandler` 是死代码 [已修 2026-09：链路接通]
- 位置：`DuiLib/Utils/WinImplBase.cpp` `HandleMessage`
- 现象：真实消息链是 `HandleMessage → (switch→OnXxx) → HandleCustomMessage →
  m_pm.MessageHandler`，`MessageHandler` 永远不会被调用。按原版 DuiLib
  文档/习惯重写它的人会静默失效。
- 修复：HandleMessage 链中、HandleCustomMessage 之后接通
  `MessageHandler`（默认实现处理 WM_KEYDOWN 的 RETURN/ESC →
  ResponseDefaultKeyEvent）。已自行在 HandleCustomMessage 拦截按键的
  宿主行为不变（其先行）；此前依赖"重写了但从未生效"现状的宿主会
  开始触发其重写体——升级注意点。

## 构建 / 工程

### 15. 无 CI [已修]
- 修复：`.github/workflows/build.yml`（windows-latest，xmake，static/shared ×
  release/debug 矩阵，产物上传）。

### 16. xmake 构建脚本缺失 [已修]
- 修复：根 `xmake.lua`（Win32/GDI 后端静态库；SDL 后端排除，走 CMake 的 Linux
  路径不受影响）。排除项完整清单与维护规则以
  `3rd/README.md`「构建排除清单（单一事实源）」为准（2026-09 起单一事实源收敛到登记表；含 unzip.cpp 经 UIFile.cpp 文本包含仍在库内的事实更正）。

### 17. 源码编码混乱 [已修（上游）]
- 上游 34cf1e3 已全量转 UTF-8 with BOM；此前 GBK 注释会导致 rg/grep 乱码、
  MSVC /utf-8 下告警。新提交请保持 UTF-8 BOM。

### 18. tests/ 仅有孤立头文件，无测试基建 [已修 2026-09]
- 修复：`tests/test_main.cpp` + 根 `xmake.lua` `target("duilib_test")`
  （binary，`set_default(false)` 不进默认构建）。零外部依赖轻量断言，
  覆盖：pugixml 独立 TU 解析/序列化（WCHAR_MODE ABI）、CDuiString、
  CDuiRect（ATL 门控触点）、控件工厂（含 ATL 门控注册集合行为面）、
  nanosvg 解析（含原 test_invariant_nanosvg.h 的 gradient-stops 不变量
  移植，孤立头文件已删除）、stb PNG 写/读内存回路。
- 运行：`xmake run duilib_test`（或 `xmake test`）。仅 `--kind=static`
  （默认）下存在——nanosvg/stb 实现位于 IRender.cpp 单 TU 且不经 DLL
  导出，shared 配置无法链接，属预期。
- 过程发现：本仓 pugixml 库文件带 `pugi`→`ui_pugi` 命名空间重命名
  （此前登记漏记），已补登记至 `3rd/pugixml/LOCAL_CHANGES.md` 与
  `3rd/README.md` 上游合并策略表。

### 19. pugixml header-only 全 TU 内联（#19） [已修 2026-09：转独立 TU]
- 原状：`DuiLib/StdAfx.h` 无条件 `#define PUGIXML_HEADER_ONLY`，所有含
  StdAfx.h 的 TU 内联一份 pugi 实现（inline COMDAT）——编译时间/体积税，
  调试器里 pugixml 栈每 TU 一份；且 `pugixml.cpp` 若被 glob 编入会因
  强符号冲突在 shared 链接 LNK2005/LNK1169。
- 修复（2026-09 收口）：`StdAfx.h` 去掉 `PUGIXML_HEADER_ONLY`，只 include
  `3rd/pugixml/pugixml.hpp` 引入声明；根 `xmake.lua` 的 `target("pugixml")`
  改为 static，独立编译 `3rd/pugixml/pugixml.cpp`（`PUGIXML_WCHAR_MODE`
  与 `_UNICODE` 头侧宏一致）。库文件魔改仅命名空间重命名
  `pugi`→`ui_pugi`（2026-09 核实登记，见
  `3rd/pugixml/LOCAL_CHANGES.md`），上游合并策略登记于
  `3rd/README.md`。
- 宿主注意：以 `DuiLib/**.cpp` glob 编源码的宿主（如 mlaunch
  DuiLibLite）**必须同步编译 `3rd/pugixml/pugixml.cpp`**（此前
  header-only 天然内联、无需此步），否则 pugi 符号无法解析。

### 20. 构建依赖 VS 的 ATL 组件（atlimage.h） [已修 2026-09：DUILIB_NO_IMAGEBOXEX / DUILIB_NO_ACTIVEX 门控]
- 位置：`DuiLib/Control/UIImageBoxEx.h:4`（经 UIlib.h 无条件包含）→ atlimage.h。
- 现象：VS 未装"适用于最新 v143 生成工具的 C++ ATL"组件时任何包含 UIlib.h
  的 TU 直接 C1083；GitHub runner 自带 ATL 所以 CI 掩盖了本机缺组件的问题。
- 门控修复（2026-09 收口）：
  - `DUILIB_NO_IMAGEBOXEX`：`UIlib.h` 的 UIImageBoxEx.h include、
    ControlFactory 的 `CImageBoxExUI` 注册、UIImageBoxEx.cpp 整文件条件化，
    定义后核心不依赖 atlimage.h；
  - `DUILIB_NO_ACTIVEX`：**文档更正**——原备注认为仅 IMAGEBOXEX 门控即可
    无 ATL 构建，实际 `UIFlash.cpp`（atlcomcli.h + CComPtr）与
    `UIWebBrowser.cpp`（atlconv.h/atlcomcli.h）同样依赖 ATL，已一并门控
    （UIlib.h include + ControlFactory 注册 + 文件级条件）。
  - 门控暴露的隐藏传递依赖已就地修复（默认构建行为不变）：
    ① `UIMsgWnd.cpp` 的 ATL `CRect` → `CDuiRect`（原经 atlimage.h→
      atltypes.h 传递可见）；
    ② `UIFile.cpp` 显式 `#include <shlwapi.h>`（原经 atlimage.h:48 传递
      引入，PathIsRelative 才能解析）；
    ③ `UIApplicationWin32.cpp` 的 ATL `CString`（3 处）→ `CDuiString`、
      `::ATL::CRegKey` RAII → 显式 `RegCloseKey`；
    ④ `TrayIcon.cpp`/`UIApplicationWin32.cpp` 显式
      `#pragma comment(lib, shell32/advapi32)`（原靠 ATL 头 defaultlib
      兜底）；xmake shared syslinks 补 `advapi32 shell32`。
  - xmake 开关：`xmake f --no_imageboxex=y --no_activex=y`（默认全关 =
    与既有行为一致），开关只加宏并移除对应 .cpp；验证见
    `compile_commands.json` 中无任何 atlmfc/atl 路径。
- 本机临时规避（历史记录）：安装 ATL 组件；注意装完需删
  `%LOCALAPPDATA%\.xmake\cache\detect` 重新探测 vstudio 环境。

## 高 DPI / 缩放

> 2026-09 专项审计。对照参考：DuiLib_Ultimate `Demos/HiDPITest`、NIM
> `DpiManager`、DUIThink `MMHelper/MMDPI`。验收工具：`Demos/HiDPITest`
> （`xmake run duilib_hidpi`，运行时切 96~192 DPI + 跨屏拖动）。
> 缩放模型备注：本库控件属性（fixedwidth/bordersize/padding/字号等）恒存
> 96 DPI 逻辑值，getter / 构建时经 `CDpiBase::ScaleInt` 缩放（抽查
> `UIControl.cpp` GetFixedWidth/GetBorderSize/GetPadding 与
> `UIObject_gdi.cpp:90` 字体构建均符合），改动不得破坏该模型。

### 22. XML `<Font default="true">` 默认字体被双重 DPI 缩放 [已修 2026-09]
- 位置：`DuiLib/Core/UIDlgBuilder.cpp`（`<Font>` 节点 defaultfont 分支）。
- 根因：builder 对 default 字体先 `GetDPIObj()->ScaleInt(size)` 再传
  `SetDefaultFont`，而 `UIFont::CreateFont` 存原始值、`_buildFont` 构建时才
  `ScaleInt`（`Render/UIObject_gdi.cpp:90` 等）→ 逻辑值位被存成已缩放值，
  构建时再放大一次。同节点上一行 `AddFont(id, ...)` 传原始 size，两条路径
  不一致；且 150% 下默认字体肉眼可见放大一档，跨档切 DPI 后字号持续错。
- 修复：builder 侧去掉预缩放（最小 diff 一行），与 AddFont 路径对齐。
- 回归门：`tests/test_main.cpp` `test_dpi_default_font_not_prescaled`
  （144 DPI 下 `GetDefaultFontInfo()->iSize == 12`，修复前为 18）；
  肉眼验证用 Demos/HiDPITest。

### 23. DPI 初始化/响应链路仅覆盖 CFrameWnd 一族 [已修 2026-09]
- 位置：`DuiLib/Core/UIManager.cpp`（SetDPI 重载 / MessageHandler / 静态开关）、
  `DuiLib/Core/UIManagerWin32.cpp`（Init）、`DuiLib/Utils/UIFrameWndWin32.cpp`
  （Create）、`DuiLib/Utils/UIFrameWnd.cpp`（去重）。
- 原状（代码层已核实）：初始 DPI 仅 `CUIFrameWndWin32::Create` 接主屏；
  `WM_DPICHANGED` 仅 `CUIFrameWndBase::HandleCustomMessage` 接，且未用 lParam
  建议矩形、SetDPI 后重复 `ResetDPIAssets()`；不经 FrameWnd 一族的窗口
  （WindowImplBase、宿主自管 CWindowWnd + CPaintManagerUI）永停 100%——
  不缩放、不响应跨屏；进程级 DPI 感知无库内入口，宿主漏调则整进程被 DWM
  虚拟化，库内一切 DPI 机制空转（位图被系统拉伸）。
- 修复（链路下沉到 manager 层，宿主零改动即获益）：
  ① `CPaintManagerUI::SetDPI(int, const CDuiRect* pSuggestedRect)` 重载——
    WM_DPICHANGED 采用 lParam 系统建议矩形（屏幕坐标），混合 DPI 多屏下比
    自算比例可靠；最大化窗口仍忽略建议矩形（与原逻辑一致）。
    原 `SetDPI(int)` 签名/行为不变，内部委托。
  ② `CPaintManagerUI::MessageHandler` 新增 `case WM_DPICHANGED`——manager 层
    统一接住跨屏 DPI 变化；`CUIFrameWndBase::HandleCustomMessage` 的重复分支
    移除（含冗余 ResetDPIAssets）。系统感知/无感知进程收不到该消息，行为不变。
  ③ opt-in 初始 DPI：`CPaintManagerUI::SetInitDPIFromMonitor(true)`（静态，
    默认 false 保持既有宿主行为）后，`CPaintManagerWin32UI::Init` 按窗口中心
    所在显示器取 DPI，SetScale 并同步放大窗口物理尺寸（宿主按 96 基准给的
    创建尺寸 × 新倍率），全程先于资源/控件解析与首次布局——首个布局即
    目标倍率，无创建期二次重排；开启期间 `CUIFrameWndWin32::Create` 不再
    以主屏 DPI 覆盖。
  ④ `CPaintManagerUI::EnablePerMonitorDPIAwareness()`——进程级 Per-Monitor
    感知一站式入口（幂等，须在任何窗口创建前调用；失败=清单/先行调用已定，
    以其为准）。非 Win32 平台恒返回 false。
  ⑤ 遗留：`CDpiBase::ScaleIntBack(ScaleInt(x))` 仅 scale>=100 恒等
    （MulDiv 对称四舍五入，test_dpi_scale_roundtrip 已盖）；scale<100 降档
    不成立——若引入 SDA 降档需先补 DUIThink MMDPI 式 ScaleVerify 往返校验。
- 回归门：test_setinitdpi_flag（opt-in 默认关 / 旧值语义）。
- 宿主接入两行示例见 `Demos/HiDPITest/main.cpp`（EnablePerMonitorDPIAwareness +
  SetInitDPIFromMonitor）；肉眼验证（含 /nodpi 对照）仍用该 demo。
- 建议方向（逐项最小 diff）：初始 DPI 与 WM_DPICHANGED 下沉到
  manager/窗口基类层（按窗口所在 monitor 取 DPI，多 manager 各自缩放）；
  WM_DPICHANGED 采用建议矩形；文档/入口明确宿主必须先 SetDPIAwareness。
- 借鉴：DUIThink `DUIWnd.cpp` 的 SetDpi→控件树 OnDpiChanged 虚链 +
  DpiChanged 通知；Ultimate HiDPITest 按窗口处理。

### 24. 多分辨率图片机制存在但受限，无矢量渲染 [待改进]
- 位置：`DuiLib/Render/IRender.cpp` `tagTDrawInfo` 构造（"调整DPI资源"块）。
- 现状：scale≠100 时按 `name@150.ext` 命名探测（`CUIFile::LoadFile` 试探），
  命中则连 `source/corner` 一起缩放（与 Ultimate 同款约定）。
- 限制：
  ① `CDuiString::Replace(_T("."), sScale)` 替换**所有**点，多点文件名
    （`icon.v2.png` → `icon@150.v2@150.png`）永远探测失败，静默回退位图
    拉伸；
  ② 未命中即拉伸原始位图，放大发糊（无分档缓存）；
  ③ 3rd/ 已有 nanosvg，但 SVG 未按 scale 实时矢量出图，切 DPI 依赖
    `ResetDPIAssets` 清缓存后按位图路径重载。
- 建议：Replace 限首个点；SVG 路径按 `GetScale()` 矢量渲染（参考 DUIThink
  `DUIImageBase` 的 `m_mapDpiImageInfo[scale]` 分档缓存，SVG 不缓存直接
  渲染）；位图维持 `@N` 约定即可。

### 25. 弹出窗口 DPI 传递无内部链路 [已修 2026-09：范围更正为 Combo]
- 原登记经复查更正：
  - **菜单弹窗本就同步**——`CMenuWndWin32` 两条创建路径在 `m_pm.Init` 后
    均 `m_pm.GetDPIObj()->SetScale(宿主 DPI)`（UIMenuWndWin32.cpp:215/251），
    公开的 `setDPI(int)` 方法确无调用方但属冗余 API，非缺陷；
  - **DateTime 弹窗不适用**——`CDateTimeWndWin32` 是原生
    SysDateTimePick32 包装（DATETIMEPICK_CLASS），无 duilib manager；
  - **Combo 下拉确认缺失**：`CComboWndWin32` manager 默认 100%。
- 修复（`UIComboWndWin32.cpp`）：
  ① WM_CREATE 中 `m_pm.Init` 后 `SetScale(宿主 DPI)`——与菜单同模式，
    不走 SetDPI 整窗缩放分支（窗口 rect 已按宿主物理像素定位），
    字体在 `_buildFont` 时按新 scale 构建；
  ② `Init` 的 `GetDropBoxSize`（逻辑值）与 EstimateSize 累计的
    `cyFixed` 按宿主 DPI 缩放——修复前下拉窗口高度恒为逻辑像素，
    >100% 主屏上内容超高被裁进滚动条。
- 验证：HiDPITest 200% 主屏下拉 Combo，下拉框高度与条目随宿主缩放。
