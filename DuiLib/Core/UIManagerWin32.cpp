#include "StdAfx.h"

#ifdef DUILIB_WIN32
#include <zmouse.h>

namespace DuiLib {

	static void GetChildWndRect(HWND hWnd, HWND hChildWnd, CDuiRect& rcChildWnd)
	{
		::GetWindowRect(hChildWnd, &rcChildWnd);

		CDuiPoint pt;
		pt.x = rcChildWnd.left;
		pt.y = rcChildWnd.top;
		::ScreenToClient(hWnd, &pt);
		rcChildWnd.left = pt.x;
		rcChildWnd.top = pt.y;

		pt.x = rcChildWnd.right;
		pt.y = rcChildWnd.bottom;
		::ScreenToClient(hWnd, &pt);
		rcChildWnd.right = pt.x;
		rcChildWnd.bottom = pt.y;
	}

	// 	static UINT MapKeyState()
	// 	{
	// 		UINT uState = 0;
	// 		if( ::GetKeyState(VK_CONTROL) < 0 ) uState |= MK_CONTROL;
	// 		if( ::GetKeyState(VK_LBUTTON) < 0 ) uState |= MK_LBUTTON;
	// 		if( ::GetKeyState(VK_RBUTTON) < 0 ) uState |= MK_RBUTTON;
	// 		if( ::GetKeyState(VK_SHIFT) < 0 ) uState |= MK_SHIFT;
	// 		if( ::GetKeyState(VK_MENU) < 0 ) uState |= MK_ALT;
	// 		return uState;
	// 	}

	typedef uiBool (__stdcall *PFUNCUPDATELAYEREDWINDOW)(HWND, HDC, CDuiPoint*, SIZE*, HDC, CDuiPoint*, COLORREF, BLENDFUNCTION*, DWORD);
	PFUNCUPDATELAYEREDWINDOW g_fUpdateLayeredWindow = NULL;

	HPEN m_hUpdateRectPen = NULL;

	/////////////////////////////////////////////////////////////////////////////////////
	//
	//
	CPaintManagerWin32UI::CPaintManagerWin32UI()
		: m_hDcPaint(NULL), m_uTimerID(0x1000)
	{
		if( m_hUpdateRectPen == NULL ) 
		{
			m_hUpdateRectPen = ::CreatePen(PS_SOLID, 1, RGB(220, 0, 0));
			// Boot Windows Common Controls (for the ToolTip control)
			::InitCommonControls();
			::LoadLibrary(_T("msimg32.dll"));
		}
	}

	CPaintManagerWin32UI::~CPaintManagerWin32UI()
	{
		if(m_pLangManager){ delete m_pLangManager; m_pLangManager = NULL; }

		// Delete the control-tree structures
		for( int i = 0; i < m_aDelayedCleanup.GetSize(); i++ ) delete static_cast<CControlUI*>(m_aDelayedCleanup[i]);
		m_aDelayedCleanup.Resize(0);
		for( int i = 0; i < m_aAsyncNotify.GetSize(); i++ ) delete static_cast<TNotifyUI*>(m_aAsyncNotify[i]);
		m_aAsyncNotify.Resize(0);

		m_mNameHash.Resize(0);
		if( m_pRoot != NULL ) delete m_pRoot;

		//delete m_ResInfo.m_DefaultFontInfo;
		RemoveAllFonts();
		RemoveAllImages();
		RemoveAllStyle();
		RemoveAllDefaultAttributeList();
		RemoveAllWindowCustomAttribute();
		RemoveAllOptionGroups();
		RemoveAllTimers();
		RemoveAllDrawInfos();

		if( m_hwndTooltip != NULL ) {
			::DestroyWindow(m_hwndTooltip);
			m_hwndTooltip = NULL;
		}
		if (!m_aFonts.IsEmpty()) {
			for (int i = 0; i < m_aFonts.GetSize();++i)
			{
				HANDLE handle = static_cast<HANDLE>(m_aFonts.GetAt(i));
				::RemoveFontMemResourceEx(handle);
			}
		}
		if( m_hDcPaint != NULL ) ::ReleaseDC(m_hWndPaint, m_hDcPaint);
		m_aPreMessages.Remove(m_aPreMessages.Find(this));


		// DPI管理对象
		if (m_pDPI != NULL) {
			delete m_pDPI;
			m_pDPI = NULL;
		}

		if(m_pRenderEngine)
		{
			//m_pRenderEngine->Release();
			m_pRenderEngine = NULL;
		}
	}

	void CPaintManagerWin32UI::Init(HWND hWnd, LPCTSTR pstrName, CWindowWnd *pWindow)
	{
		ASSERT(::IsWindow(hWnd));
		CPaintManagerUI::Init(hWnd, pstrName, pWindow);
		if( m_hWndPaint != hWnd ) 
		{
			m_hWndPaint = hWnd;
			m_pWindow = pWindow;
			m_hDcPaint = ::GetDC(hWnd);
			m_aPreMessages.Add(this);
			Render()->Init(this, m_hDcPaint);
		}
	}

	UIRender *CPaintManagerWin32UI::Render()
	{
		if(!m_pRenderEngine)
		{
			m_pRenderEngine = MakeRefPtr<UIRender>(UIGlobal::CreateRenderTarget());
			m_pRenderEngine->Init(this, m_hDcPaint);
		}
		ASSERT(m_pRenderEngine);
		return m_pRenderEngine;
	}


	uiBool CPaintManagerWin32UI::InvalidateRect(UIWND hWnd, const CDuiRect*lpRect, uiBool bErase)
	{
		return ::InvalidateRect(hWnd, lpRect, bErase);
	}

	HDC CPaintManagerWin32UI::GetPaintDC() const
	{
		return m_hDcPaint;
	}

	void CPaintManagerWin32UI::SetLayered(bool bLayered)
	{
		if( m_hWndPaint != NULL && bLayered != m_bLayered ) {
			UINT uStyle = GetWindowStyle(m_hWndPaint);
			if( (uStyle & WS_CHILD) != 0 ) return;
			if( g_fUpdateLayeredWindow == NULL ) {
				HMODULE hUser32 = ::GetModuleHandle(_T("User32.dll"));
				if (hUser32) {
					g_fUpdateLayeredWindow = 
						(PFUNCUPDATELAYEREDWINDOW)::GetProcAddress(hUser32, "UpdateLayeredWindow");
					if( g_fUpdateLayeredWindow == NULL ) return;
				}
			}
			m_bLayered = bLayered;
			if( m_pRoot != NULL ) m_pRoot->NeedUpdate();
			Invalidate();
		}
	}

	void CPaintManagerWin32UI::SetCursor(int nCursor)
	{
		::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(nCursor)));
	}

	static WORD DIBNumColors(void* pv) 
	{     
		int bits;     
		LPBITMAPINFOHEADER  lpbi;     
		LPBITMAPCOREHEADER  lpbc;      
		lpbi = ((LPBITMAPINFOHEADER)pv);     
		lpbc = ((LPBITMAPCOREHEADER)pv);      
		/*  With the BITMAPINFO format headers, the size of the palette 
		*  is in biClrUsed, whereas in the BITMAPCORE - style headers, it      
		*  is dependent on the bits per pixel ( = 2 raised to the power of      
		*  bits/pixel).
		*/     
		if (lpbi->biSize != sizeof(BITMAPCOREHEADER))
		{         
			if (lpbi->biClrUsed != 0)
				return (WORD)lpbi->biClrUsed;         
			bits = lpbi->biBitCount;     
		}     
		else         
			bits = lpbc->bcBitCount;
		switch (bits)
		{         
		case 1:                 
			return 2;         
		case 4:                 
			return 16;         
		case 8:       
			return 256;
		default:
			/* A 24 bitcount DIB has no color table */                 
			return 0;
		} 
	} 

	//code taken from SEEDIB MSDN sample
	static WORD ColorTableSize(LPVOID lpv)
	{
		LPBITMAPINFOHEADER lpbih = (LPBITMAPINFOHEADER)lpv;

		if (lpbih->biSize != sizeof(BITMAPCOREHEADER))
		{
			if (((LPBITMAPINFOHEADER)(lpbih))->biCompression == BI_BITFIELDS)
				/* Remember that 16/32bpp dibs can still have a color table */
				return (sizeof(DWORD) * 3) + (DIBNumColors (lpbih) * sizeof (RGBQUAD));
			else
				return (WORD)(DIBNumColors (lpbih) * sizeof (RGBQUAD));
		}
		else
			return (WORD)(DIBNumColors (lpbih) * sizeof (RGBTRIPLE));
	}

	void CPaintManagerWin32UI::SetCapture()
	{
		::SetCapture(m_hWndPaint);
		m_bMouseCapture = true;
	}

	void CPaintManagerWin32UI::ReleaseCapture()
	{
		::ReleaseCapture();
		m_bMouseCapture = false;
	}

	bool CPaintManagerWin32UI::IsCaptured()
	{
		return m_bMouseCapture;
	}

	uiBool CPaintManagerWin32UI::SetTimer(UINT uElapse, TIMERINFO* pTimer)
	{
		m_uTimerID = (++m_uTimerID) % 0xF0; //0xf1-0xfe特殊用途
		pTimer->uWinTimer = m_uTimerID;
		return ::SetTimer(pTimer->hWnd, pTimer->uWinTimer, uElapse, NULL) != NULL;
	}

	uiBool CPaintManagerWin32UI::KillTimer(TIMERINFO* pTimer)
	{
		if (pTimer == NULL) return uiFalse;
		if (!::IsWindow(pTimer->hWnd))
			return uiFalse;
		return ::KillTimer(pTimer->hWnd, pTimer->uWinTimer);
	}

	uiBool CPaintManagerWin32UI::GetCursorPos(LPPOINT pt)
	{
		if(!::GetCursorPos(pt)) return uiFalse;
		return uiTrue;
	}

	uiBool CPaintManagerWin32UI::ScreenToClient(LPPOINT pt)
	{
		if(!::ScreenToClient(GetPaintWindow(), pt)) return uiFalse;
		return uiTrue;
	}

	void CPaintManagerWin32UI::SetWndFocus()
	{
		::SetFocus(m_hWndPaint);
	}

	UIWND CPaintManagerWin32UI::GetWndFocus()
	{
		return ::GetFocus();
	}

	uiBool CPaintManagerWin32UI::IsZoomed()
	{
		return ::IsZoomed(m_hWndPaint);
	}

	uiBool CPaintManagerWin32UI::IsIconic()
	{
		return ::IsIconic(m_hWndPaint);
	}

	uiBool CPaintManagerWin32UI::SetWindowPos(int x, int y, int cx, int cy, UINT uFlags)
	{
		if (!IsWindow()) return uiFalse;
		return ::SetWindowPos(m_hWndPaint, NULL, x, y, cx, cy, uFlags);
	}

	uiBool CPaintManagerWin32UI::GetWindowRect(LPRECT lpRect)
	{
		if (!IsWindow()) return uiFalse;
		return ::GetWindowRect(m_hWndPaint, lpRect);
	}

	uiBool CPaintManagerWin32UI::GetClientRect(LPRECT lpRect)
	{
		if (!IsWindow()) return uiFalse;
		return ::GetClientRect(m_hWndPaint, lpRect);
	}

	uiBool CPaintManagerWin32UI::IsKeyDown(UINT uKey)
	{
		return ::GetKeyState(uKey) < 0;
	}

	uiBool CPaintManagerWin32UI::IsKeyUp(UINT uKey)
	{
		return ::GetKeyState(uKey) >= 0;
	}

	uiBool CPaintManagerWin32UI::IsCtrlKeyDown()		{ return ::GetKeyState(VK_CONTROL)	< 0; }
	uiBool CPaintManagerWin32UI::IsAltKeyDown()		{ return ::GetKeyState(VK_MENU)		< 0; }
	uiBool CPaintManagerWin32UI::IsShiftKeyDown()		{ return ::GetKeyState(VK_SHIFT)	< 0; }
	uiBool CPaintManagerWin32UI::IsCapsLockKeyOn()	{ return ::GetKeyState(VK_CAPITAL)	< 0; }
	uiBool CPaintManagerWin32UI::IsNumberLockKeyOn()	{ return ::GetKeyState(VK_NUMLOCK)	< 0; }

	UINT CPaintManagerWin32UI::MapKeyState()
	{
		UINT uState = 0;
		if( IsKeyDown(VK_CONTROL) ) uState |= MK_CONTROL;
		if( IsKeyDown(VK_LBUTTON) ) uState |= MK_LBUTTON;
		if( IsKeyDown(VK_RBUTTON) ) uState |= MK_RBUTTON;
		if( IsKeyDown(VK_SHIFT) ) uState |= MK_SHIFT;
		if( IsKeyDown(VK_MENU) ) uState |= MK_ALT;
		return uState;
	}

	DWORD CPaintManagerWin32UI::GetTickCount()
	{
		return ::GetTickCount();
	}

	void CPaintManagerWin32UI::GetLocalTime(SYSTEMTIME &st)
	{
		return ::GetLocalTime(&st);
	}

	int CPaintManagerWin32UI::GetNativeWindowCount() const
	{
		return m_aNativeWindow.GetSize();
	}

	bool CPaintManagerWin32UI::AddNativeWindow(CControlUI* pControl, HWND hChildWnd)
	{
		if (pControl == NULL || hChildWnd == NULL) return false;

		CDuiRect rcChildWnd = GetNativeWindowRect(hChildWnd);
		Invalidate(rcChildWnd);

		if (m_aNativeWindow.Find(hChildWnd) >= 0) return false;
		if (m_aNativeWindow.Add(hChildWnd)) {
			m_aNativeWindowControl.Add(pControl);
			return true;
		}
		return false;
	}

	bool CPaintManagerWin32UI::RemoveNativeWindow(HWND hChildWnd)
	{
		for( int i = 0; i < m_aNativeWindow.GetSize(); i++ ) {
			if( static_cast<HWND>(m_aNativeWindow[i]) == hChildWnd ) {
				if( m_aNativeWindow.Remove(i) ) {
					m_aNativeWindowControl.Remove(i);
					return true;
				}
				return false;
			}
		}
		return false;
	}

	CDuiRect CPaintManagerWin32UI::GetNativeWindowRect(HWND hChildWnd)
	{
		CDuiRect rcChildWnd;
		::GetWindowRect(hChildWnd, &rcChildWnd);
		::ScreenToClient(m_hWndPaint, (LPPOINT)(&rcChildWnd));
		::ScreenToClient(m_hWndPaint, (LPPOINT)(&rcChildWnd)+1);
		return rcChildWnd;
	}

	void CPaintManagerWin32UI::MessageLoop()
	{
		MSG msg = { 0 };
		while( ::GetMessage(&msg, NULL, 0, 0) ) 
		{
			if( !CPaintManagerWin32UI::TranslateMessage(&msg) ) 
			{
				::TranslateMessage(&msg);
				try
				{
					::DispatchMessage(&msg);
				} 
				catch(...) 
				{
					DUITRACE(_T("EXCEPTION: %s(%d)\n"), __FILET__, __LINE__);
				}
			}
		}
	}

	bool CPaintManagerWin32UI::OnClose(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		TEventUI event = { 0 };
		event.ptMouse = m_ptLastMousePos;
		event.wKeyState = MapKeyState();
		event.dwTimestamp = ::GetTickCount();
		if( m_pEventHover != NULL ) {
			event.Type = UIEVENT_MOUSELEAVE;
			event.pSender = m_pEventHover;
			m_pEventHover->Event(event);
		}
		if( m_pEventClick != NULL ) {
			event.Type = UIEVENT_BUTTONUP;
			event.pSender = m_pEventClick;
			m_pEventClick->Event(event);
		}

		SetFocus(NULL);

		if( ::GetActiveWindow() == m_hWndPaint ) {
			HWND hwndParent = ::GetWindow(m_hWndPaint, GW_OWNER);//GetWindowOwner(m_hWndPaint);
			if( hwndParent != NULL ) ::SetFocus(hwndParent);
		}

		if (m_hwndTooltip != NULL) {
			::DestroyWindow(m_hwndTooltip);
			m_hwndTooltip = NULL;
		}
		return false;
	}

	bool CPaintManagerWin32UI::OnPaint(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		if( m_pRoot == NULL ) {
			PAINTSTRUCT ps = { 0 };
			::BeginPaint(m_hWndPaint, &ps);
			//Render()->DrawColor( ps.rcPaint, 0xFF000000);
			::EndPaint(m_hWndPaint, &ps);
			return true;
		}

		CDuiRect rcClient;
		::GetClientRect(m_hWndPaint, &rcClient);

		//RECT rcPaint = { 0 };
		CDuiRect rcPaint;
		if( m_bLockUpdate )
		{
			// 宿主在拖拽/缩放期间挂起重绘：只校验更新区域不绘制，
			// 让 DWM 拉伸显示旧帧，WM_EXITSIZEMOVE 后统一重排（去闪烁）。
			PAINTSTRUCT ps = { 0 };
			::BeginPaint(m_hWndPaint, &ps);
			::EndPaint(m_hWndPaint, &ps);
			return true;
		}
		if( !::GetUpdateRect(m_hWndPaint, &rcPaint, uiFalse) )
			return true;

		// Set focus to first control?
		if( m_bFocusNeeded ) {
			SetNextTabControl();
		}

		bool bNeedSizeMsg = false;
		DWORD dwWidth = rcClient.right - rcClient.left;
		DWORD dwHeight = rcClient.bottom - rcClient.top;

		SetPainting(true);
		if( m_bUpdateNeeded && !m_bLockUpdate) 
		{
			m_bUpdateNeeded = false;
			if( !rcClient.IsEmpty() && !::IsIconic(m_hWndPaint) ) 
			{
				if( m_pRoot->IsUpdateNeeded() ) 
				{
					CDuiRect rcRoot = rcClient;
					if( m_bLayered ) 
					{
						rcRoot.left += m_rcLayeredInset.left;
						rcRoot.top += m_rcLayeredInset.top;
						rcRoot.right -= m_rcLayeredInset.right;
						rcRoot.bottom -= m_rcLayeredInset.bottom;
					}
					m_pRoot->SetPos(rcRoot, true);
					bNeedSizeMsg = true;
					// 重排会移动/显隐子控件，局部 update region 会留下陈旧像素
					// （BeginPaint 会校验整个 region 但只绘制 rcPaint），这里强制整窗重绘。
					rcPaint = rcClient;
				}
				else 
				{
					//单独NeedUpdate某个控件时
					CControlUI* pControl = NULL;
					m_aFoundControls.Empty();
					m_pRoot->FindControl(__FindControlsFromUpdate, NULL, UIFIND_VISIBLE | UIFIND_ME_FIRST | UIFIND_UPDATETEST);
					for( int it = 0; it < m_aFoundControls.GetSize(); it++ ) {
						pControl = static_cast<CControlUI*>(m_aFoundControls[it]);
						//float控件不需要重新计算RelativePos，因为第一次SetPos时，已经把偏移计算好了，并且保存到m_rcItem。
						//if( !pControl->IsFloat() ) pControl->SetPos(pControl->GetPos(), true);
						//else pControl->SetPos(pControl->GetRelativePos(), true);
						pControl->SetPos(pControl->GetPos(), true);
					}
					bNeedSizeMsg = true;
				}

				//第一次绘制窗口之前，发送DUI_MSGTYPE_WINDOWINIT消息
				if( m_bFirstLayout ) 
				{
					m_bFirstLayout = false;
					SendNotify(m_pRoot, DUI_MSGTYPE_WINDOWINIT,  0, 0, false);
					if( m_bLayered && m_bLayeredChanged ) 
					{
						Invalidate();
						SetPainting(false);
						return true;
					}
					// 更新阴影窗口显示
					m_shadow.Update(m_hWndPaint);
				}
			}
		}
		else if( m_bLayered && m_bLayeredChanged ) {
			CDuiRect rcRoot = rcClient;
			rcRoot.left += m_rcLayeredInset.left;
			rcRoot.top += m_rcLayeredInset.top;
			rcRoot.right -= m_rcLayeredInset.right;
			rcRoot.bottom -= m_rcLayeredInset.bottom;
			m_pRoot->SetPos(rcRoot, true);
		}

		if( m_bLayered ) 
		{
			DWORD dwExStyle = ::GetWindowLong(m_hWndPaint, GWL_EXSTYLE);
			DWORD dwNewExStyle = dwExStyle | WS_EX_LAYERED;
			if(dwExStyle != dwNewExStyle) ::SetWindowLong(m_hWndPaint, GWL_EXSTYLE, dwNewExStyle);

			//UnionRect(&rcPaint, &rcPaint, &m_rcLayeredUpdate);
			rcPaint.Union(rcPaint, m_rcLayeredUpdate);
			if( rcPaint.right > rcClient.right ) rcPaint.right = rcClient.right;
			if( rcPaint.bottom > rcClient.bottom ) rcPaint.bottom = rcClient.bottom;
			//::ZeroMemory(&m_rcLayeredUpdate, sizeof(m_rcLayeredUpdate));
			m_rcLayeredUpdate.Empty();
		}

		PAINTSTRUCT ps = { 0 };
		Render()->BeginPaint();
		::BeginPaint(m_hWndPaint, &ps);

		Render()->Resize(dwWidth, dwHeight);

		Render()->ClearAlpha(rcPaint);

		Render()->SaveDC();

		if( m_bLayered ) 
		{
			if(!m_diLayered.sDrawString.IsEmpty() && IsLayeredChanged()) 
			{
				DWORD dwWidth = rcClient.right - rcClient.left;
				DWORD dwHeight = rcClient.bottom - rcClient.top;
				CDuiRect rcLayeredClient = rcClient;
				rcLayeredClient.left += m_rcLayeredInset.left;
				rcLayeredClient.top += m_rcLayeredInset.top;
				rcLayeredClient.right -= m_rcLayeredInset.right;
				rcLayeredClient.bottom -= m_rcLayeredInset.bottom;

				UIClip clip;
				clip.GenerateClip(Render(), rcLayeredClient);
				Render()->DrawImageInfo(rcLayeredClient, rcLayeredClient, &m_diLayered);
			}
		}

		GetRoot()->Paint(Render(), rcPaint, NULL);

		for( int i = 0; i < m_aPostPaintControls.GetSize(); i++ ) {
			CControlUI* pPostPaintControl = static_cast<CControlUI*>(m_aPostPaintControls[i]);
			pPostPaintControl->DoPostPaint(Render(), rcPaint);
		}

		//画出需要刷新的区域，可以用于某些测试场景。
		if( IsShowUpdateRect() && !IsLayered() ) 
		{
			Render()->DrawRect(rcPaint, 1, CDuiColor::Red);
		}

		//Render()->GetBitmap()->SaveFile(_T("c:\\uiframe.bmp"));
		Render()->RestoreDC();

		if( IsLayered() ) 
		{
			CDuiRect rcWnd;
			::GetWindowRect(m_hWndPaint, &rcWnd);
			BLENDFUNCTION bf = { AC_SRC_OVER, 0, GetOpacity(), AC_SRC_ALPHA };
			CDuiPoint ptPos = rcWnd.LeftTop();
			CDuiSize sizeWnd = rcClient;
			CDuiPoint ptSrc;
			::UpdateLayeredWindow(m_hWndPaint, NULL, &ptPos, &sizeWnd, Render()->GetDC(), &ptSrc, 0, &bf, ULW_ALPHA);
		}
		else 
		{
			::BitBlt(m_hDcPaint, rcPaint.left, rcPaint.top, rcPaint.right - rcPaint.left, rcPaint.bottom - rcPaint.top, Render()->GetDC(), rcPaint.left, rcPaint.top, SRCCOPY);
		}

		Render()->RestoreObject();
		::EndPaint(m_hWndPaint, &ps);
		Render()->EndPaint();

		// 绘制结束
		SetPainting(false);
		m_bLayeredChanged = false;
		if( m_bUpdateNeeded ) Invalidate();

		// 发送窗口大小改变消息
		if(bNeedSizeMsg) {
			this->SendNotify(m_pRoot, DUI_MSGTYPE_WINDOWSIZE, 0, 0, true);
		}
		return true;
	}

	bool CPaintManagerWin32UI::OnEraseBkgnd(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		lRes = 1;
		return false;
	}

	bool CPaintManagerWin32UI::OnPrintClient(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		// 		if( m_pRoot == NULL ) return false;
		// 		RECT rcClient;
		// 		::GetClientRect(m_hWndPaint, &rcClient);
		// 		HDC hDC = (HDC) wParam;
		// 		int save = ::SaveDC(hDC);
		// 		m_pRoot->Paint(hDC, rcClient, NULL);
		// 		if( (lParam & PRF_CHILDREN) != 0 ) {
		// 			HWND hWndChild = ::GetWindow(m_hWndPaint, GW_CHILD);
		// 			while( hWndChild != NULL ) {
		// 				RECT rcPos = { 0 };
		// 				::GetWindowRect(hWndChild, &rcPos);
		// 				::MapWindowPoints(HWND_DESKTOP, m_hWndPaint, reinterpret_cast<LPPOINT>(&rcPos), 2);
		// 				::SetWindowOrgEx(hDC, -rcPos.left, -rcPos.top, NULL);
		// 				::SendMessage(hWndChild, WM_PRINT, wParam, lParam | PRF_NONCLIENT);
		// 				hWndChild = ::GetWindow(hWndChild, GW_HWNDNEXT);
		// 			}
		// 		}
		// 		::RestoreDC(hDC, save);
		return false;
	}

	bool CPaintManagerWin32UI::OnGetMaxMinInfo(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		MONITORINFO Monitor = {};
		Monitor.cbSize = sizeof(Monitor);
		::GetMonitorInfo(::MonitorFromWindow(m_hWndPaint, MONITOR_DEFAULTTOPRIMARY), &Monitor);
		//RECT rcWork = Monitor.rcWork;
		CDuiRect rcWork = Monitor.rcWork;
		if( Monitor.dwFlags != MONITORINFOF_PRIMARY ) {
			//::OffsetRect(&rcWork, -rcWork.left, -rcWork.top);
			rcWork.Offset(-rcWork.left, -rcWork.top);
		}

		LPMINMAXINFO lpMMI = (LPMINMAXINFO) lParam;
		if( m_szMinWindow.cx > 0 ) lpMMI->ptMinTrackSize.x = m_szMinWindow.cx;
		if( m_szMinWindow.cy > 0 ) lpMMI->ptMinTrackSize.y = m_szMinWindow.cy;
		if( m_szMaxWindow.cx > 0 ) lpMMI->ptMaxTrackSize.x = m_szMaxWindow.cx;
		if( m_szMaxWindow.cy > 0 ) lpMMI->ptMaxTrackSize.y = m_szMaxWindow.cy;
		if( m_szMaxWindow.cx > 0 ) lpMMI->ptMaxSize.x = m_szMaxWindow.cx;
		if( m_szMaxWindow.cy > 0 ) lpMMI->ptMaxSize.y = m_szMaxWindow.cy;

		return false;
	}

	bool CPaintManagerWin32UI::OnMouseOver(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		m_bMouseTracking = false;
		CDuiPoint pt(lParam);
		CControlUI* pHover = FindControl(pt);
		if( pHover == NULL ) return false;
		// Generate mouse hover event
		if( m_pEventHover != NULL ) {
			TEventUI event = { 0 };
			event.Type = UIEVENT_MOUSEHOVER;
			event.pSender = m_pEventHover;
			event.wParam = wParam;
			event.lParam = lParam;
			event.dwTimestamp = ::GetTickCount();
			event.ptMouse = pt;
			event.wKeyState = MapKeyState();
			m_pEventHover->Event(event);
		}
		// Create tooltip information
		CDuiString sToolTip = pHover->GetToolTip();
		if( sToolTip.IsEmpty() ) return true;
		::ZeroMemory(&m_ToolTip, sizeof(TOOLINFO));
		m_ToolTip.cbSize = sizeof(TOOLINFO);
		m_ToolTip.uFlags = TTF_IDISHWND;
		m_ToolTip.hwnd = m_hWndPaint;
		m_ToolTip.uId = (UINT_PTR) m_hWndPaint;
		m_ToolTip.hinst = m_hInstance;
		m_ToolTip.lpszText = const_cast<LPTSTR>( (LPCTSTR) sToolTip );
		m_ToolTip.rect = pHover->GetPos();
		if( m_hwndTooltip == NULL ) {
			m_hwndTooltip = ::CreateWindowEx(0, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hWndPaint, NULL, m_hInstance, NULL);
			::SendMessage(m_hwndTooltip, TTM_ADDTOOL, 0, (LPARAM) &m_ToolTip);
			::SendMessage(m_hwndTooltip,TTM_SETMAXTIPWIDTH,0, pHover->GetToolTipWidth());
		}
		if(!::IsWindowVisible(m_hwndTooltip))
		{
			::SendMessage(m_hwndTooltip, TTM_SETTOOLINFO, 0, (LPARAM)&m_ToolTip);
			::SendMessage(m_hwndTooltip, TTM_TRACKACTIVATE, uiTrue, (LPARAM)&m_ToolTip);
		}

		return true;
	}

	bool CPaintManagerWin32UI::OnMouseLeave(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		if( m_hwndTooltip != NULL ) ::SendMessage(m_hwndTooltip, TTM_TRACKACTIVATE, uiFalse, (LPARAM) &m_ToolTip);
		if( m_bMouseTracking ) {
			CDuiPoint pt;
			CDuiRect rcWnd;
			::GetCursorPos(&pt);
			::GetWindowRect(m_hWndPaint, &rcWnd);
			//if( !::IsIconic(m_hWndPaint) && ::GetActiveWindow() == m_hWndPaint && ::PtInRect(&rcWnd, pt) ) {
			if( !::IsIconic(m_hWndPaint) && ::GetActiveWindow() == m_hWndPaint && rcWnd.PtInRect(pt) ) {
				if( ::SendMessage(m_hWndPaint, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y)) == HTCLIENT ) {
					::ScreenToClient(m_hWndPaint, &pt);
					::SendMessage(m_hWndPaint, WM_MOUSEMOVE, 0, MAKELPARAM(pt.x, pt.y));
				}
				else 
					::SendMessage(m_hWndPaint, WM_MOUSEMOVE, 0, (LPARAM)-1);
			}
			else 
				::SendMessage(m_hWndPaint, WM_MOUSEMOVE, 0, (LPARAM)-1);
		}
		m_bMouseTracking = false;

		return false;
	}

	bool CPaintManagerWin32UI::OnMouseMove(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		// Start tracking this entire window again...
		if( !m_bMouseTracking ) {
			TRACKMOUSEEVENT tme = { 0 };
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_HOVER | TME_LEAVE;
			tme.hwndTrack = m_hWndPaint;
			tme.dwHoverTime = m_hwndTooltip == NULL ? m_iTooltipHoverTime : (DWORD) ::SendMessage(m_hwndTooltip, TTM_GETDELAYTIME, TTDT_INITIAL, 0L);
			_TrackMouseEvent(&tme);
			m_bMouseTracking = true;
		}

		// Generate the appropriate mouse messages
		CDuiPoint pt(lParam);
		m_ptLastMousePos = pt;
		CControlUI* pNewHover = FindControl(pt);
		if( pNewHover != NULL && pNewHover->GetManager() != this ) return false;

		TEventUI event = { 0 };
		event.ptMouse = pt;
		event.wParam = wParam;
		event.lParam = lParam;
		event.dwTimestamp = ::GetTickCount();
		event.wKeyState = MapKeyState();
		if( !IsCaptured() ) {
			pNewHover = FindControl(pt);
			if( pNewHover != NULL && pNewHover->GetManager() != this ) return false;
			if( pNewHover != m_pEventHover && m_pEventHover != NULL ) {
				event.Type = UIEVENT_MOUSELEAVE;
				event.pSender = m_pEventHover;

				CStdPtrArray aNeedMouseLeaveNeeded(m_aNeedMouseLeaveNeeded.GetSize());
				aNeedMouseLeaveNeeded.Resize(m_aNeedMouseLeaveNeeded.GetSize());
				::CopyMemory(aNeedMouseLeaveNeeded.GetData(), m_aNeedMouseLeaveNeeded.GetData(), m_aNeedMouseLeaveNeeded.GetSize() * sizeof(LPVOID));
				for( int i = 0; i < aNeedMouseLeaveNeeded.GetSize(); i++ ) {
					static_cast<CControlUI*>(aNeedMouseLeaveNeeded[i])->Event(event);
				}

				m_pEventHover->Event(event);
				m_pEventHover = NULL;
				if( m_hwndTooltip != NULL ) ::SendMessage(m_hwndTooltip, TTM_TRACKACTIVATE, uiFalse, (LPARAM) &m_ToolTip);
			}
			if( pNewHover != m_pEventHover && pNewHover != NULL ) {
				event.Type = UIEVENT_MOUSEENTER;
				event.pSender = pNewHover;
				pNewHover->Event(event);
				m_pEventHover = pNewHover;
			}
		}
		if( m_pEventClick != NULL ) {
			event.Type = UIEVENT_MOUSEMOVE;
			event.pSender = m_pEventClick;
			m_pEventClick->Event(event);
		}
		else if( pNewHover != NULL ) {
			event.Type = UIEVENT_MOUSEMOVE;
			event.pSender = pNewHover;
			pNewHover->Event(event);
		}

		return false;
	}

	bool CPaintManagerWin32UI::OnMouseWheel(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		if( m_pRoot == NULL ) return false;
		CDuiPoint pt(lParam);
		::ScreenToClient(m_hWndPaint, &pt);
		m_ptLastMousePos = pt;
		CControlUI* pControl = FindControl(pt);
		if( pControl == NULL ) return false;
		if( pControl->GetManager() != this ) return false;
		int zDelta = (int) (short) HIWORD(wParam);
		TEventUI event = { 0 };
		event.Type = UIEVENT_SCROLLWHEEL;
		event.pSender = pControl;
		event.wParam = MAKELPARAM(zDelta < 0 ? SB_LINEDOWN : SB_LINEUP, zDelta);
		event.lParam = lParam;
		event.ptMouse = pt;
		event.wKeyState = MapKeyState();
		event.dwTimestamp = ::GetTickCount();
		pControl->Event(event);

		// Let's make sure that the scroll item below the cursor is the same as before...
		::SendMessage(m_hWndPaint, WM_MOUSEMOVE, 0, (LPARAM) MAKELPARAM(m_ptLastMousePos.x, m_ptLastMousePos.y));

		return false;
	}

	bool CPaintManagerWin32UI::OnContextMenu(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		if( m_pRoot == NULL ) return false;
		CDuiPoint pt(lParam);
		::ScreenToClient(m_hWndPaint, &pt);
		m_ptLastMousePos = pt;
		if( m_pEventClick == NULL ) return false;
		ReleaseCapture();
		TEventUI event = { 0 };
		event.Type = UIEVENT_CONTEXTMENU;
		event.pSender = m_pEventClick;
		event.wParam = wParam;
		event.lParam = lParam;
		event.ptMouse = pt;
		event.wKeyState = (WORD)wParam;
		event.lParam = (LPARAM)m_pEventClick;
		event.dwTimestamp = ::GetTickCount();
		m_pEventClick->Event(event);
		m_pEventClick = NULL;

		return false;
	}

	bool CPaintManagerWin32UI::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		if( lParam == 0 ) return false;
		LPNMHDR lpNMHDR = (LPNMHDR) lParam;
		if( lpNMHDR != NULL ) lRes = ::SendMessage(lpNMHDR->hwndFrom, OCM__BASE + WM_NOTIFY, wParam, lParam);
		return true;
	}

	bool CPaintManagerWin32UI::OnCommand(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		if( lParam == 0 ) return false;
		HWND hWndChild = (HWND) lParam;
		lRes = ::SendMessage(hWndChild, OCM__BASE + WM_COMMAND, wParam, lParam);
		if(lRes != 0) return true;
		return false;
	}

	bool CPaintManagerWin32UI::OnCtlColorEdit(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		// Refer To: http://msdn.microsoft.com/en-us/library/bb761691(v=vs.85).aspx
		// Read-only or disabled edit controls do not send the WM_CTLCOLOREDIT message; instead, they send the WM_CTLCOLORSTATIC message.
		if( lParam == 0 ) return false;
		HWND hWndChild = (HWND) lParam;
		lRes = ::SendMessage(hWndChild, OCM__BASE + WM_CTLCOLOREDIT, wParam, lParam);
		if(lRes != 0) return true;
		return false;
	}

	bool CPaintManagerWin32UI::OnCtlColorStatic(WPARAM wParam, LPARAM lParam, LRESULT& lRes)
	{
		// Refer To: http://msdn.microsoft.com/en-us/library/bb761691(v=vs.85).aspx
		// Read-only or disabled edit controls do not send the WM_CTLCOLOREDIT message; instead, they send the WM_CTLCOLORSTATIC message.
		if( lParam == 0 ) return false;
		HWND hWndChild = (HWND) lParam;
		lRes = ::SendMessage(hWndChild, OCM__BASE + WM_CTLCOLORSTATIC, wParam, lParam);
		if(lRes != 0) return true;
		return false;
	}


	UIBitmap* CPaintManagerWin32UI::CreateControlBitmap(CControlUI* pControl, CDuiColor dwFilterColor, CControlUI* pStopControl)
	{
		CPaintManagerUI *pManager = pControl->GetManager();
		if(pManager == NULL) return uiFalse;
		if(pManager->GetRoot() == NULL) return uiFalse;

		CDuiRect rcControl = pControl->GetPos();
		int cx = rcControl.right - rcControl.left;
		int cy = rcControl.bottom - rcControl.top;

		//建立新的绘图
		CStdRefPtr<UIRender> pRender = MakeRefPtr<UIRender>(UIGlobal::CreateRenderTarget());
		pRender->Init(pManager);

		//注意这里必须是root的rc，或者整个client的pos。 总之这个size必须包含了rcControl
		pRender->Resize(pManager->GetRoot()->GetPos()); 

		//在整个区域只绘制了pControl
		pControl->Paint(pRender, rcControl, NULL);

		//再建立一个新的绘图， 把绘制好的pControl的区域BitBlt过来
		CStdRefPtr<UIRender> pRenderClone = MakeRefPtr<UIRender>(UIGlobal::CreateRenderTarget());
		pRenderClone->Init(pManager);

		UIBitmap *pBitmapClone = UIGlobal::CreateBitmap();
		pBitmapClone->CreateARGB32Bitmap(pManager->GetPaintDC(), cx, cy, uiTrue);
		pRenderClone->SelectObject(pBitmapClone);

		pRenderClone->BitBlt(0, 0, cx, cy, pRender, rcControl.left, rcControl.top, SRCCOPY);
		if (dwFilterColor > 0x00FFFFFF) 
			pRenderClone->DrawColor(CDuiRect(0,0,cx,cy), CDuiSize(0,0), dwFilterColor);

		return pBitmapClone;
	}

	//原理是，整个窗口画一遍，除了pControl自己不画，然后rcWnd的位置截图下来，就是内部窗口的背景图了。
	//这样就可以实现Edit的透明背景了
	UIBitmap* CPaintManagerWin32UI::CreateControlBackBitmap(CControlUI* pControl, const CDuiRect&rcWnd, CDuiColor dwFilterColor)
	{
		CPaintManagerUI *pManager = pControl->GetManager();
		if(pManager == NULL) return uiFalse;
		if(pManager->GetRoot() == NULL) return uiFalse;

		CControlUI *pRoot = pManager->GetRoot();
		CDuiRect rcRoot = pRoot->GetPos();

		int cx = rcWnd.right - rcWnd.left;
		int cy = rcWnd.bottom - rcWnd.top;

		//建立新的绘图
		CStdRefPtr<UIRender> pRender = MakeRefPtr<UIRender>(UIGlobal::CreateRenderTarget());
		pRender->Init(pManager);
		pRender->Resize(rcRoot); 

		//从root开始绘制，遇到pControl时，停止绘制
		pRoot->Paint(pRender, rcWnd, pControl);

		//再建立一个新的绘图， 把绘制好的rcWnd的区域BitBlt过来
		CStdRefPtr<UIRender> pRenderClone = MakeRefPtr<UIRender>(UIGlobal::CreateRenderTarget());
		pRenderClone->Init(pManager);

		//创建返回的位图，调用方需要释放。
		UIBitmap *pBitmapClone = UIGlobal::CreateBitmap();
		pBitmapClone->CreateARGB32Bitmap(pManager->GetPaintDC(), cx, cy, uiTrue);
		pRenderClone->SelectObject(pBitmapClone);
		pRenderClone->BitBlt(0, 0, cx, cy, pRender, rcWnd.left, rcWnd.top, SRCCOPY);

		if (dwFilterColor > 0x00FFFFFF) 
			pRenderClone->DrawColor(CDuiRect(0,0,cx,cy), CDuiSize(0,0), dwFilterColor);

		return pBitmapClone;
	}

	//////////////////////////////////////////////////////////////////////////
	CLockWindowUpdateUI::CLockWindowUpdateUI(CPaintManagerUI* pManager) : m_pManager(pManager)
	{
		if (::IsWindow(m_pManager->GetPaintWindow()))
		{
			//::LockWindowUpdate(m_pManager->GetPaintWindow());
			SetWindowRedraw(pManager->GetPaintWindow(), uiFalse);
		}
	}
	CLockWindowUpdateUI::~CLockWindowUpdateUI()
	{
		if (::IsWindow(m_pManager->GetPaintWindow()))
		{
			//::LockWindowUpdate(NULL);
			SetWindowRedraw(m_pManager->GetPaintWindow(), uiTrue);
			m_pManager->Invalidate();
		}
	}

} // namespace DuiLib
#endif //#ifdef DUILIB_WIN32

