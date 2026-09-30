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
- `/nodpi` 诊断开关：`duilib_hidpi.exe /nodpi` 跳过基类 `Create` 末尾的
  `SetDPI(主屏 DPI)`，用于区分"DPI 初始化问题"与"绘制链路问题"
  （实测两者均正常，黑屏为上述 XML 约定踩坑）。
- 窗口派生自 `CFrameWnd`（`CUIFrameWndWin32`）一族，初始 DPI 与
  `WM_DPICHANGED` 均由基类处理（对照 KNOWN_ISSUES #23 记录的覆盖范围：
  非 CFrameWnd 窗口不在此列）。
- 进程级 DPI 感知由本 Demo 在首个窗口前显式 `SetDPIAwareness`
  （KNOWN_ISSUES #23-④：库不代调，宿主职责）。
- 不进默认构建（`set_default(false)`），经根 `xmake.lua` 的
  `includes("Demos/HiDPITest/xmake.lua")` 接入，仅 Win32 后端。
