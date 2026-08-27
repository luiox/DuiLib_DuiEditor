#include "StdAfx.h"
#include "UIEditWndWin32.h"

#ifdef DUILIB_WIN32
namespace DuiLib
{
	CEditWndWin32::CEditWndWin32(CControlUI *pOwner) : CEditWnd(pOwner), m_hBkBrush(NULL), m_bDrawCaret(false)
	{
		
	}

	void CEditWndWin32::Init()
	{
		CDuiRect rcPos = CalPos();
		UINT uStyle = 0;
		if(m_pOwner->GetManager()->IsLayered()) {
			uStyle = WS_POPUP | ES_AUTOHSCROLL | WS_VISIBLE;
			CDuiRect rcWnd;
			::GetWindowRect(m_pOwner->GetManager()->GetPaintWindow(), &rcWnd);
			rcPos.left += rcWnd.left;
			rcPos.right += rcWnd.left;
			rcPos.top += rcWnd.top - 1;
			rcPos.bottom += rcWnd.top - 1;
		}
		else {
			uStyle = WS_CHILD | ES_AUTOHSCROLL;
		}
		UINT uTextStyle = m_pOwner->GetTextStyle();
		if(uTextStyle & DT_LEFT) uStyle |= ES_LEFT;
		else if(uTextStyle & DT_CENTER) uStyle |= ES_CENTER;
		else if(uTextStyle & DT_RIGHT) uStyle |= ES_RIGHT;
		if( IsPasswordMode() ) uStyle |= ES_PASSWORD;
		if( IsMultiLine()) uStyle |= ES_MULTILINE | ES_AUTOVSCROLL;
		if( IsWantReturn()) uStyle |= ES_WANTRETURN;
		Create(m_pOwner->GetManager()->GetPaintWindow(), NULL, uStyle, 0, rcPos.left, rcPos.top, rcPos.GetWidth(), rcPos.GetHeight());

		m_font = MakeRefPtr<UIFont>(m_pOwner->GetManager()->CloneFont(m_pOwner->GetFont()));
		::SendMessage(m_hWnd, WM_SETFONT, (WPARAM)m_font->GetHFONT(m_pOwner->GetManager()), (LPARAM)uiTrue);
		Edit_LimitText(m_hWnd, GetMaxChar());
		if( IsPasswordMode() ) Edit_SetPasswordChar(m_hWnd, GetPasswordChar());
		Edit_SetText(m_hWnd, m_pOwner->GetText());
		Edit_SetModify(m_hWnd, uiFalse);
		::SendMessage(m_hWnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
		Edit_Enable(m_hWnd, m_pOwner->IsEnabled() == true);
		Edit_SetReadOnly(m_hWnd, m_pOwner->IsReadOnly() == true);

		//Styls
		LONG styleValue = ::GetWindowLong(m_hWnd, GWL_STYLE);
		IsNumberOnly()		? styleValue |= ES_NUMBER		: styleValue &= ~ES_NUMBER;
		IsLowerCase()		? styleValue |= ES_LOWERCASE	: styleValue &= ~ES_LOWERCASE;
		IsUpperCase()		? styleValue |= ES_UPPERCASE	: styleValue &= ~ES_UPPERCASE;	
		IsPasswordMode()	? styleValue |= ES_PASSWORD		: styleValue &= ~ES_PASSWORD;	

		::SetWindowLong(GetHWND(), GWL_STYLE, styleValue);

		::ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
		::SetFocus(m_hWnd);
		if (IsAutoSelAll()) 
		{
			int nSize = GetWindowTextLength(m_hWnd);
			if( nSize == 0 ) nSize = 1;
			Edit_SetSel(m_hWnd, 0, nSize);
		}
		else {
			int nSize = GetWindowTextLength(m_hWnd);
			Edit_SetSel(m_hWnd, nSize, nSize);
		}
	}

	CDuiRect CEditWndWin32::CalPos()
	{
		CDuiRect rcPos = m_pOwner->GetClientPos();
		rcPos.Deflate(m_pOwner->GetTextPadding());
		if(m_pOwner && !IsMultiLine())
		{
			LONG lEditHeight = m_pOwner->GetManager()->GetFontHeight(m_pOwner->GetFont());
			if( lEditHeight < rcPos.GetHeight() ) {
				rcPos.top += (rcPos.GetHeight() - lEditHeight) / 2;
				rcPos.bottom = rcPos.top + lEditHeight;
			}
		}

		CControlUI* pParent = m_pOwner;
		CDuiRect rcParent;
		while( pParent = pParent->GetParent() ) {
			if( !pParent->IsVisible() ) {
				rcPos.left = rcPos.top = rcPos.right = rcPos.bottom = 0;
				break;
			}
			rcParent = pParent->GetClientPos();
			//if( !::IntersectRect(&rcPos, &rcPos, &rcParent) ) {
			if( !rcPos.Intersect(rcPos, rcParent) ) {
				rcPos.left = rcPos.top = rcPos.right = rcPos.bottom = 0;
				break;
			}
		}

		return rcPos;
	}

	void CEditWndWin32::SetEditText(LPCTSTR sText)
	{
		if(::IsWindow(m_hWnd))
			Edit_SetText(m_hWnd, sText)	;
	}

	LPCTSTR CEditWndWin32::GetWindowClassName() const
	{
		return _T("EditWnd");
	}

	LPCTSTR CEditWndWin32::GetSuperClassName() const
	{
		return WC_EDIT;
	}

	void CEditWndWin32::OnFinalMessage(HWND hWnd)
	{
		m_pOwner->Invalidate();
		// Clear reference and die
		if( m_hBkBrush != NULL ) ::DeleteObject(m_hBkBrush);

		CPaintManagerWin32UI *pManager = (CPaintManagerWin32UI *)m_pOwner->GetManager();
		if (pManager->IsLayered()) 
		{
			pManager->RemoveNativeWindow(hWnd);		}

		delete this;
	}

	LRESULT CEditWndWin32::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		LRESULT lRes = 0;
		uiBool bHandled = uiTrue;
		if( uMsg == WM_CREATE ) 
		{
			//m_pOwner->GetManager()->AddNativeWindow(m_pOwner, m_hWnd);
			//if( m_pOwner->GetManager()->IsLayered() ) {
			//	::SetTimer(m_hWnd, DEFAULT_TIMERID, ::GetCaretBlinkTime(), NULL);
			//}
			bHandled = uiFalse;
		}
		else if( uMsg == WM_KILLFOCUS ) lRes = OnKillFocus(uMsg, wParam, lParam, bHandled);
		else if( uMsg == OCM_COMMAND ) {
			if( GET_WM_COMMAND_CMD(wParam, lParam) == EN_CHANGE ) lRes = OnEditChanged(uMsg, wParam, lParam, bHandled);
			else if( GET_WM_COMMAND_CMD(wParam, lParam) == EN_UPDATE ) {
				CDuiRect rcClient;
				::GetClientRect(m_hWnd, &rcClient);
				::InvalidateRect(m_hWnd, &rcClient, uiFalse);
			}
		}
		else if( uMsg == WM_KEYDOWN && TCHAR(wParam) == VK_RETURN )
		{
			if(!IsWantReturn())
				m_pOwner->GetManager()->SendNotify(m_pOwner, DUI_MSGTYPE_RETURN);
		}
		else if( uMsg == WM_KEYDOWN && TCHAR(wParam) == VK_TAB ){
			if (m_pOwner->GetManager()->IsLayered()) {
				m_pOwner->GetManager()->SetNextTabControl();
			}
		}
		else if( uMsg == OCM__BASE + WM_CTLCOLOREDIT  || uMsg == OCM__BASE + WM_CTLCOLORSTATIC ) 
		{
			//if (m_pOwner->GetManager()->IsLayered() && !m_pOwner->GetManager()->IsPainting()) {
			//	m_pOwner->GetManager()->AddNativeWindow(m_pOwner, m_hWnd);
			//}
			CDuiColor clrColor = GetNativeEditBkColor();
			if( clrColor == 0xFFFFFFFF ) 
				return 0;
			::SetBkMode((HDC)wParam, TRANSPARENT);
			CDuiColor dwTextColor = m_pOwner->GetTextColor();
			::SetTextColor((HDC)wParam, RGB(GetBValue(dwTextColor),GetGValue(dwTextColor),GetRValue(dwTextColor)));
			return (LRESULT)m_brush->GetHBrush();
		}
		else if( uMsg == WM_SIZE)
		{
			//OutputDebugString(_T("InitNativeRender \r\n"));
			CDuiColor clrColor = GetNativeEditBkColor();
			if (clrColor < 0xFF000000) 
			{
				CPaintManagerWin32UI *pManager = (CPaintManagerWin32UI *)m_pOwner->GetManager();
				CDuiRect rcWnd = pManager->GetNativeWindowRect(m_hWnd);
				m_bmpBrush = MakeRefPtr<UIBitmap>(pManager->CreateControlBackBitmap(m_pOwner, rcWnd, clrColor));
				m_brush = MakeRefPtr<UIBrush>(UIGlobal::CreateBrush());
				m_brush->CreateBitmapBrush(m_bmpBrush);
			}
			else 
			{
				m_brush = MakeRefPtr<UIBrush>(UIGlobal::CreateBrush());
				m_brush->CreateSolidBrush(clrColor);
			}
		}
		else if( uMsg == WM_PAINT) {
			//if (m_pOwner->GetManager()->IsLayered()) {
			//	m_pOwner->GetManager()->AddNativeWindow(m_pOwner, m_hWnd);
			//}
			bHandled = uiFalse;
		}
		else if( uMsg == WM_PRINT ) {
			//if (m_pOwner->GetManager()->IsLayered()) {
			//	lRes = CWindowWnd::HandleMessage(uMsg, wParam, lParam);
			//	if( m_pOwner->IsEnabled() && m_bDrawCaret ) {
			//		RECT rcClient;
			//		::GetClientRect(m_hWnd, &rcClient);
			//		POINT ptCaret;
			//		::GetCaretPos(&ptCaret);
			//		RECT rcCaret = { ptCaret.x, ptCaret.y, ptCaret.x, ptCaret.y+rcClient.bottom-rcClient.top };
			//		CRenderEngine::DrawLine((HDC)wParam, rcCaret, 1, 0xFF000000);
			//	}
			//	return lRes;
			//}
			bHandled = uiFalse;
		}
		else if( uMsg == WM_TIMER ) {
			if (wParam == CARET_TIMERID) {
				m_bDrawCaret = !m_bDrawCaret;
				CDuiRect rcClient;
				::GetClientRect(m_hWnd, &rcClient);
				::InvalidateRect(m_hWnd, &rcClient, uiFalse);
				return 0;
			}
			bHandled = uiFalse;
		}
		else bHandled = uiFalse;

		if( !bHandled ) return DefaultWndProc(uMsg, wParam, lParam);
		return lRes;
	}

	LRESULT CEditWndWin32::OnKillFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, uiBool& bHandled)
	{
		LRESULT lRes = ::DefWindowProc(m_hWnd, uMsg, wParam, lParam);
		//if ((HWND)wParam != m_pOwner->GetManager()->GetPaintWindow()) {
		//	::SendMessage(m_pOwner->GetManager()->GetPaintWindow(), WM_KILLFOCUS, wParam, lParam);
		//}
		::PostMessage(m_hWnd, WM_CLOSE, 0, 0);
		return lRes;
	}

	LRESULT CEditWndWin32::OnEditChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, uiBool& /*bHandled*/)
	{
		if( m_pOwner == NULL ) return 0;
		// Copy text back
		int cchLen = ::GetWindowTextLength(m_hWnd) + 1;
		LPTSTR pstr = static_cast<LPTSTR>(_alloca(cchLen * sizeof(TCHAR)));
		ASSERT(pstr);
		if( pstr == NULL ) return 0;
		::GetWindowText(m_hWnd, pstr, cchLen);
		SetOwnerText(pstr);
		m_pOwner->GetManager()->SendNotify(m_pOwner, DUI_MSGTYPE_TEXTCHANGED);
		if( m_pOwner->GetManager()->IsLayered() ) m_pOwner->Invalidate();
		return 0;
	}
}
#endif // #ifdef DUILIB_WIN32



