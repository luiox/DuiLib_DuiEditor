#include <tchar.h>
// UILIB_STATIC 由 demo target 的 add_defines 提供（与 duilib_test 同口径）
#include "UIlib.h"

// Demos/HiDPITest/main.cpp —— DPI 验证窗（KNOWN_ISSUES #22-#25 的验收工具）
//
// 验证点：
//   1. 运行时点击 96/120/144/168/192 按钮切 DPI（SetAllDPI）：默认字体、
//      固定尺寸控件、边框/文本内边距应整体等比缩放；
//   2. 跨显示器拖动窗口：CFrameWnd 一族应收到 WM_DPICHANGED 并自适应
//      （KNOWN_ISSUES #23 记录的覆盖范围）；
//   3. XML <Font default> 字号在 150% 下不得异常放大（#22 修复的肉眼验证）。
// 皮肤为内嵌 XML 字符串（CDialogBuilder 对 '<' 开头的字符串直接 load_string），
// 零资源文件依赖。
//
// 构建：xmake build duilib_hidpi && xmake run duilib_hidpi

using namespace DuiLib;

namespace {

const TCHAR* kSkinXml =
	// 注意 duilib 的 skin 约定：根元素只是包装节点（CDialogBuilder::_Parse 遍历其
	// 子元素，第一个子元素才是窗口根控件，其余兄弟节点被丢弃），因此全部行必须
	// 包在下面这一个内层 VerticalLayout 里，bkcolor 也要写在内层上。
	_T("<VerticalLayout>")
	_T("  <VerticalLayout name='root' bkcolor='#FFF2F2F2'>")
	_T("  <HorizontalLayout height='56'>")
	_T("    <Label width='12'/>")
	_T("    <Label name='lbldpi' text='当前 DPI: ?' width='300' height='32' textcolor='#FF1A1A1A'/>")
	_T("  </HorizontalLayout>")
	_T("  <HorizontalLayout height='44'>")
	_T("    <Label width='12'/>")
	_T("    <Button name='btn_96' text='96' width='64' height='32' normalbkcolor='#FF4472C4' hotbkcolor='#FF5B9BD5' bordersize='1' bordercolor='#FF2F5597'/>")
	_T("    <Label width='6'/>")
	_T("    <Button name='btn_120' text='120' width='64' height='32' bkcolor='#FF4472C4' hotbkcolor='#FF5B9BD5' bordersize='1' bordercolor='#FF2F5597'/>")
	_T("    <Label width='6'/>")
	_T("    <Button name='btn_144' text='144' width='64' height='32' bkcolor='#FF4472C4' hotbkcolor='#FF5B9BD5' bordersize='1' bordercolor='#FF2F5597'/>")
	_T("    <Label width='6'/>")
	_T("    <Button name='btn_168' text='168' width='64' height='32' bkcolor='#FF4472C4' hotbkcolor='#FF5B9BD5' bordersize='1' bordercolor='#FF2F5597'/>")
	_T("    <Label width='6'/>")
	_T("    <Button name='btn_192' text='192' width='64' height='32' bkcolor='#FF4472C4' hotbkcolor='#FF5B9BD5' bordersize='1' bordercolor='#FF2F5597'/>")
	_T("    <Label width='16'/>")
	_T("    <Label text='点击切换 DPI（等效跨屏拖动）' height='32' textcolor='#FF888888'/>")
	_T("  </HorizontalLayout>")
	_T("  <HorizontalLayout height='52'>")
	_T("    <Label width='12'/>")
	_T("    <Label text='边框 2px / 文本内边距 8px' width='340' height='44' bkcolor='#FFFFFFFF' bordersize='2' bordercolor='#FF3C78AA' textpadding='8,4,8,4'/>")
	_T("  </HorizontalLayout>")
	_T("  <HorizontalLayout height='44'>")
	_T("    <Label width='12'/>")
	_T("    <Edit name='edit' text='输入框' width='300' height='32' bkcolor='#FFFFFFFF' bordersize='1' bordercolor='#FF9A9A9A'/>")
	_T("    <Label width='10'/>")
	_T("    <Combo name='combo' dropboxsize='320,220' width='300' height='32' bkcolor='#FFFFFFFF' bordersize='1' bordercolor='#FF9A9A9A' textpadding='6,0,6,0'>")
	_T("      <ListLabelElement text='下拉项 DPI 同步验证'/>")
	_T("      <ListLabelElement text='选项二'/>")
	_T("      <ListLabelElement text='选项三'/>")
	_T("      <ListLabelElement text='选项四'/>")
	_T("    </Combo>")
	_T("  </HorizontalLayout>")
	_T("  <HorizontalLayout height='160'>")
	_T("    <Label width='12'/>")
	_T("    <VerticalLayout width='260' bkcolor='#FFEAF1F8' bordersize='1' bordercolor='#FF7F9DB9'>")
	_T("      <Label text='高 30' height='30' bkcolor='#FFE8A33D'/>")
	_T("      <Label text='高 30' height='30' bkcolor='#FF67B168'/>")
	_T("      <Label text='高 30' height='30' bkcolor='#FFDD7E6B'/>")
	_T("      <Label text='高 30' height='30' bkcolor='#FFFFD966'/>")
	_T("      <Label text='弹性填充区' bkcolor='#FFEAF1F8'/>")
	_T("    </VerticalLayout>")
	_T("    <Label width='10'/>")
	_T("    <VerticalLayout width='340' bkcolor='#FFFBEED5' bordersize='1' bordercolor='#FFB7945F'>")
	_T("      <Button text='fixed 180 x 36' width='180' height='36' bkcolor='#FF70AD47'/>")
	_T("      <Button text='fixed 180 x 36' width='180' height='36' bkcolor='#FF5B9BD5'/>")
	_T("      <Label text='弹性填充区' bkcolor='#FFFBEED5'/>")
	_T("    </VerticalLayout>")
	_T("  </HorizontalLayout>")
	_T("  <HorizontalLayout height='60'>")
	_T("    <Label width='12'/>")
	_T("    <Label text='跨屏拖动窗口应收到 WM_DPICHANGED 并整体缩放；运行中直接改显示设置不发此消息（KNOWN_ISSUES #23）。' height='40' textcolor='#FF888888'/>")
	_T("  </HorizontalLayout>")
	_T("  </VerticalLayout>")
	_T("</VerticalLayout>");

class CHidpiWindow : public CUIFrameWndWin32
{
public:
	virtual LPCTSTR GetWindowClassName() const override { return _T("DuiHidpiTestWnd"); }
	virtual CDuiString GetSkinFile() override { return CDuiString(kSkinXml); }

	// 对照实验开关：/nodpi 时跳过基类 Create 末尾的 SetDPI(主屏 DPI 兜底)，
	// 验证"创建期 SetDPI 是否破坏首次全量布局"。正常路径已由 #23-④ opt-in
	// 在 Init 阶段按所在屏设定,基类兜底自动让位(_tWinMain 按开关设好)。
	virtual UIWND Create(UIWND hwndParent, LPCTSTR pstrName, DWORD dwStyle, DWORD dwExStyle, int x, int y, int cx, int cy) override
	{
		if (GetModuleHandle(NULL) && __argc > 1 && _tcsstr(__targv[1], _T("/nodpi")) != NULL) {
			return CWindowWin32::Create(hwndParent, pstrName, dwStyle, dwExStyle, x, y, cx, cy);
		}
		return CUIFrameWndWin32::Create(hwndParent, pstrName, dwStyle, dwExStyle, x, y, cx, cy);
	}

	virtual void InitWindow() override { UpdateDpiLabel(); }

	// SetDPI 完成时（含 CFrameWnd 创建期的初始 DPI、跨屏 WM_DPICHANGED、
	// 本 demo 的按钮切换三条路径）都会 PostMessage(UIMSG_SET_DPI)，
	// 在此统一刷新标签，避免 InitWindow 早于初始 SetDPI 的时序问题。
	virtual LRESULT HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, uiBool& bHandled) override
	{
		if (uMsg == UIMSG_SET_DPI) {
			UpdateDpiLabel();
			return 0;
		}
		return CUIFrameWndWin32::HandleCustomMessage(uMsg, wParam, lParam, bHandled);
	}

	virtual void OnNotifyClick(TNotifyUI& msg) override
	{
		static const struct { LPCTSTR name; int dpi; } kBtns[] = {
			{ _T("btn_96"), 96 },  { _T("btn_120"), 120 }, { _T("btn_144"), 144 },
			{ _T("btn_168"), 168 }, { _T("btn_192"), 192 },
		};
		for (int i = 0; i < (int)(sizeof(kBtns) / sizeof(kBtns[0])); ++i) {
			if (IsControl(msg, kBtns[i].name)) {
				GetManager()->SetAllDPI(kBtns[i].dpi);
				UpdateDpiLabel();
				return;
			}
		}
	}

private:
	void UpdateDpiLabel()
	{
		CControlUI* p = GetManager()->FindControl(_T("lbldpi"));
		if (p == NULL) return;
		CDuiString s;
		s.Format(_T("当前 DPI: %d  (scale %d%%)"),
			(int)GetManager()->GetDPIObj()->GetDPI(),
			(int)GetManager()->GetDPIObj()->GetScale());
		p->SetText(s.GetData());
	}
};

} // namespace

int APIENTRY _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
{
	bool bNoDpi = (GetModuleHandle(NULL) && __argc > 1 && _tcsstr(__targv[1], _T("/nodpi")) != NULL);

	// #23-⑤ 新入口:进程级 Per-Monitor DPI 感知,替代旧的"临时 manager 调 SetDPIAwareness"绕法。
	// 必须先于首个窗口建立(漏调则整进程被 DWM 虚拟化,一切库内 DPI 机制空转)。
	CPaintManagerUI::EnablePerMonitorDPIAwareness();

	// #23-④ opt-in:窗口初建即按所在显示器 DPI 初始化(替代基类 Create 末尾的主屏 DPI);
	// /nodpi 对照实验保持旧行为——跳过库内一切创建期 DPI 干预
	CPaintManagerUI::SetInitDPIFromMonitor(!bNoDpi);

	CHidpiWindow wnd;
	wnd.Create(NULL, _T("DuiLib HiDPI Test"),
		UI_WNDSTYLE_FRAME, WS_EX_WINDOWEDGE, 0, 0, 900, 640);
	wnd.CenterWindow();
	wnd.ShowWindow();
	CPaintManagerWin32UI::MessageLoop();
	return 0;
}
