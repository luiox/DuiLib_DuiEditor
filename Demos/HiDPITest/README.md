# HiDPITest —— DPI 验证窗

`docs/KNOWN_ISSUES.md`「高 DPI / 缩放」分节（#22-#25）的验收工具：
运行时切换 96/120/144/168/192 DPI，肉眼验证字体、固定尺寸控件、边框、
文本内边距的整体缩放一致性；跨显示器拖动验证 `WM_DPICHANGED` 链路。

```powershell
xmake build duilib_hidpi
xmake run duilib_hidpi
```

- 零资源文件：皮肤是内嵌 XML 字符串（`CDialogBuilder` 对 `<` 开头的
  字符串直接 `load_string`）。
- **skin XML 根元素约定**：根元素只是包装节点——`CDialogBuilder::_Parse`
  遍历根元素的子元素，**第一个子元素**才是窗口根控件，其余兄弟节点被
  丢弃；根元素自身的属性（如 bkcolor）也被忽略。实测踩坑记录（表现为
  全黑窗口 + 只有一行文字）。
- `/nodpi` 诊断开关：`duilib_hidpi.exe /nodpi` 关闭 #23-④ opt-in（库内
  创建期一切 DPI 干预跳过、基类主屏兜底也不做），用于区分"DPI 初始化
  问题"与"绘制链路问题"。
- 本 demo 即 KNOWN_ISSUES #23 修复后的宿主接入两行示例：首个窗口前
  `CPaintManagerUI::EnablePerMonitorDPIAwareness()`（进程级 Per-Monitor
  感知）+ `CPaintManagerUI::SetInitDPIFromMonitor(true)`（窗口初建即按
  所在显示器取 DPI 并同步放大窗口物理尺寸，先于皮肤解析/首次布局）。
  `WM_DPICHANGED` 跨屏变化由 `CPaintManagerUI::MessageHandler` 统一处理
  （采用系统建议矩形），任何接通 m_pm 消息链的窗口都自动受益；运行时
  切换仍可用窗口上的 DPI 按钮组。
- 注意：`WM_DPICHANGED` 是系统专用消息，`PostMessage`/`SendMessage`
  手工投递一律失败（实测同进程也返回 0）——链路只能靠真实跨屏拖动或
  系统改 DPI 触发，无法脚本注入验证。
- 不进默认构建（`set_default(false)`），经根 `xmake.lua` 的
  `includes("Demos/HiDPITest/xmake.lua")` 接入，仅 Win32 后端。
