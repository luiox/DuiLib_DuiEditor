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

### 3. `CControlUI::NeedUpdate` 对不可见控件直接早退 [待修]
- 位置：`DuiLib/Core/UIControl.cpp:1069`
- 现象：`NeedUpdate()` 开头 `if(!IsVisible()) return;`——控件在隐藏状态下被
  标记更新会被丢弃，之后 `SetVisible(true)` 若无显式 NeedUpdate 不会重绘。
  宿主被迫在显隐切换处手工 `GetRoot()->NeedUpdate()`。
- 建议方案：不可见时仍置 `m_bUpdateNeeded = true`，仅跳过 Invalidate。

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

### 12. CButtonUI 不支持 normalbkcolor/hotbkcolor/pushedbkcolor [待修]
- 位置：`DuiLib/Control/UIButton.cpp:247` `SetAttribute`
- 现象：属性被静默忽略（落 CLabelUI），按钮无底色/悬停态。XML 里写了不报错，
  极易误用。
- 建议方案：SetAttribute 增加三态底色 + PaintStatusImage 自绘纯色
  （宿主 appui::ButtonUI 已验证该实现，可直接下沉入库）。

### 13. CCheckBoxUI 视觉态依赖图片资源 [待修]
- 现象：无 selectedstateimage 等资源时复选框无可见勾选态。
- 建议方案：无图片时回退自绘（边框 + 对勾），与 #12 一并处理。

### 14. `WindowImplBase::MessageHandler` 是死代码 [待修/兼容性]
- 位置：`DuiLib/Utils/WinImplBase.cpp:64`
- 现象：真实消息链是 `HandleMessage → (switch→OnXxx) → HandleCustomMessage
  (:402) → m_pm.MessageHandler(:405)`，`MessageHandler` 永远不会被调用。
  按原版 DuiLib 文档/习惯重写它的人会静默失效。
- 建议方案：HandleMessage 链中调用 MessageHandler，或删除该方法避免误导。

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

### 18. tests/ 仅有孤立头文件，无测试基建 [待修]
- 现状：`tests/test_invariant_nanosvg.h` 无 runner。
- 建议：加一个最小 xmake test target（编译期断言或 gtest 单测），CI 里跑。

### 19. pugixml header-only 全 TU 内联（#19） [已修 2026-09：转独立 TU]
- 原状：`DuiLib/StdAfx.h` 无条件 `#define PUGIXML_HEADER_ONLY`，所有含
  StdAfx.h 的 TU 内联一份 pugi 实现（inline COMDAT）——编译时间/体积税，
  调试器里 pugixml 栈每 TU 一份；且 `pugixml.cpp` 若被 glob 编入会因
  强符号冲突在 shared 链接 LNK2005/LNK1169。
- 修复（2026-09 收口）：`StdAfx.h` 去掉 `PUGIXML_HEADER_ONLY`，只 include
  `3rd/pugixml/pugixml.hpp` 引入声明；根 `xmake.lua` 的 `target("pugixml")`
  改为 static，独立编译 `3rd/pugixml/pugixml.cpp`（`PUGIXML_WCHAR_MODE`
  与 `_UNICODE` 头侧宏一致）。库文件零魔改，上游合并策略登记于
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
