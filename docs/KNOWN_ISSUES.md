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

### 2. LockUpdate 期间仍实际执行绘制，DWM 显示旧帧被拉伸 [已修]
- 位置：同文件 `OnPaint`
- 现象：宿主在 WM_ENTRYSIZEMOVE/WM_SIZING 期间 `SetLockUpdate(true)` 想去闪烁，
  但绘制路径照走，DWM 拉伸旧帧反而闪烁。
- 修复：`m_bLockUpdate` 时仅 BeginPaint/EndPaint 校验更新区域，不绘制；
  WM_EXITSIZEMOVE 后由宿主统一重排。

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

## 输入 / 焦点

### 5. 原生 EDIT 画刷在 WM_SIZE 重复重建 → 背景黑块 [已修]
- 位置：`DuiLib/Control/UIEditWndWin32.cpp` `HandleMessage` WM_SIZE 分支
- 现象：固定色路径每次 WM_SIZE 重建 solid brush 并释放旧刷；EDIT 缓存了
  WM_CTLCOLOREDIT 返回的旧句柄，下次绘制用到已删除句柄 → 整条字高带纯黑。
- 修复：固定色与尺寸无关，仅在 `m_brush == nullptr` 时创建一次。
- 遗留：`clrColor < 0xFF000000`（带 alpha，走 CreateControlBackBitmap）分支
  仍在每次 WM_SIZE 重建 bitmap brush 并释放旧刷，理论上存在同源黑块风险；
  mlaunch 未使用该路径，暂无实测场景，先记录 [待修]。

### 6. 原生 EDIT 吞掉 ESC，宿主收不到 [已修]
- 位置：同文件 WM_KEYDOWN 链
- 现象：EDIT 持有焦点时 WM_KEYDOWN 只到 EDIT，ESC 无法让宿主退出搜索等模式。
- 修复：EDIT 内 ESC → `SendNotify(owner, _T("escape"))` 转通知。

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
  路径不受影响）。排除项与理由见脚本头注释。

### 17. 源码编码混乱 [已修（上游）]
- 上游 34cf1e3 已全量转 UTF-8 with BOM；此前 GBK 注释会导致 rg/grep 乱码、
  MSVC /utf-8 下告警。新提交请保持 UTF-8 BOM。

### 18. tests/ 仅有孤立头文件，无测试基建 [待修]
- 现状：`tests/test_invariant_nanosvg.h` 无 runner。
- 建议：加一个最小 xmake test target（编译期断言或 gtest 单测），CI 里跑。

### 19. pugixml.cpp 作为独立 TU 与 StdAfx 的 PUGIXML_HEADER_ONLY 冲突 [已修]
- 位置：`DuiLib/StdAfx.h:82` 无条件 `#define PUGIXML_HEADER_ONLY`，
  所有含 StdAfx.h 的 TU 内联一份 pugi 实现（inline COMDAT）；
  `DuiLib/Utils/pugixml/pugixml.cpp` 独立编译时无此宏、产出强符号。
- 现象：`add_files("DuiLib/**.cpp")` 类 glob 会把 pugixml.cpp 编进库——
  shared 链接必 LNK2005/LNK1169；static 下也是一份冗余实现。
- 修复：xmake.lua 排除 `DuiLib/Utils/pugixml/pugixml.cpp`。宿主侧以 glob
  编源码时同样需要排除（mlaunch DuiLibLite 已同步）。

### 20. 构建依赖 VS 的 ATL 组件（atlimage.h） [规避]
- 位置：`DuiLib/Control/UIImageBoxEx.h`（经 UIlib.h 无条件包含）→ atlimage.h。
- 现象：VS 未装"适用于最新 v143 生成工具的 C++ ATL"组件时任何包含 UIlib.h
  的 TU 直接 C1083；GitHub runner 自带 ATL 所以 CI 掩盖了本机缺组件的问题。
- 规避：本机安装 ATL 组件（mlaunch 实测 14.44 工具集路径
  `VC\Tools\MSVC\<ver>\atlmfc\include`）。注意 VS 组件装完后 xmake 需删
  `%LOCALAPPDATA%\.xmake\cache\detect` 重新探测 vstudio 环境，`xmake f -c`
  （项目级）清不掉该缓存。
- 备注：若未来需要无 ATL 构建，可给 UIImageBoxEx.h / ControlFactory 的
  CImageBoxExUI 注册加 `DUILIB_NO_IMAGEBOXEX` 门控（mlaunch 不使用该控件）。
