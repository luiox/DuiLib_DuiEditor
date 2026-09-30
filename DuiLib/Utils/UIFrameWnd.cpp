#include "StdAfx.h"
#include <algorithm>

namespace DuiLib{

CUIFrameWndBase::CUIFrameWndBase(void) : m_pApplication(NULL)
{
	m_vctStaticName.push_back(_T("controlui"));
	m_vctStaticName.push_back(_T("textui"));
	m_vctStaticName.push_back(_T("labelui"));
	m_vctStaticName.push_back(_T("containerui"));
	m_vctStaticName.push_back(_T("horizontallayoutui"));
	m_vctStaticName.push_back(_T("verticallayoutui"));
	m_vctStaticName.push_back(_T("tablayoutui"));
	m_vctStaticName.push_back(_T("childlayoutui"));
	m_vctStaticName.push_back(_T("dialoglayoutui"));
	m_vctStaticName.push_back(_T("progresscontainerui"));
}

CUIFrameWndBase::~CUIFrameWndBase(void)
{
	
}

DuiLibPaintManagerUI *CUIFrameWndBase::GetManager()
{
	return &m_pm;
}

void CUIFrameWndBase::OnFinalMessage( UIWND hWnd )
{
	for (int i=0; i<m_listForm.GetSize(); i++)
	{
		CUIForm *pForm = (CUIForm *)m_listForm.GetAt(i);
		delete pForm;
	}
	m_listForm.Empty();

	GetManager()->RemovePreMessageFilter(this);
	GetManager()->RemoveNotifier(this);
	GetManager()->ReapObjects(GetManager()->GetRoot());
}

CControlUI *CUIFrameWndBase::GetRoot()
{
	return GetManager()->GetRoot();
}

void CUIFrameWndBase::AttachVirtualForm(CUIFrmBase *pForm)
{
	CUIForm *p = dynamic_cast<CUIForm *>(pForm);
	if(!p) return;

	for (int i=0; i<m_listForm.GetSize(); i++)
	{
		CUIForm *pForm1 = (CUIForm *)m_listForm.GetAt(i);
		if(pForm1 == pForm)
			return;
	}

	p->SetFrameWnd(this);
	m_listForm.Add(pForm);
}

void CUIFrameWndBase::DetachVirtualForm(CUIFrmBase *pForm)
{
	for (int i=0; i<m_listForm.GetSize(); i++)
	{
		CUIForm *pForm1 = (CUIForm *)m_listForm.GetAt(i);
		if(pForm1 == pForm)
		{
			m_listForm.Remove(i);
			return;
		}
	}
}

void CUIFrameWndBase::Notify(TNotifyUI& msg)
{
	if (msg.sType == DUI_MSGTYPE_CLICK)
	{
		CDuiString sCtrlName = msg.pSender->GetName();
		if (sCtrlName == _T("windowclosebtn"))
		{
			Close(IDCANCEL);
		}
		else if (sCtrlName == _T("windowminbtn"))
		{
			DuiLibWindowWnd::SendMessage(m_hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
		}
		else if (sCtrlName == _T("windowmaxbtn"))
		{
			DuiLibWindowWnd::SendMessage(m_hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
		}
		else if (sCtrlName == _T("windowrestorebtn"))
		{
			DuiLibWindowWnd::SendMessage(m_hWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
		}
	}

	else if (msg.sType == DUI_MSGTYPE_WINDOWINIT)
	{
		if (GetManager()->IsZoomed())
		{
			CControlUI* pControl = static_cast<CControlUI*>(GetManager()->FindControl(_T("windowmaxbtn")));
			if (pControl) pControl->SetVisible(false);
			pControl = static_cast<CControlUI*>(GetManager()->FindControl(_T("windowrestorebtn")));
			if (pControl) pControl->SetVisible(true);
		}
		else
		{
			CControlUI* pControl = static_cast<CControlUI*>(GetManager()->FindControl(_T("windowmaxbtn")));
			if (pControl) pControl->SetVisible(true);
			pControl = static_cast<CControlUI*>(GetManager()->FindControl(_T("windowrestorebtn")));
			if (pControl) pControl->SetVisible(false);
		}
	}

	else if (msg.sType == DUI_MSGTYPE_TABACTIVEFORM)
	{
		for (int i = 0; i < m_listForm.GetSize(); i++)
		{
			CUIForm* pForm = (CUIForm*)m_listForm.GetAt(i);
			if (pForm->IsForm(msg.pSender->GetName()))
			{
				pForm->OnActiveForm();
				return;
			}
		}
	}

	else if (msg.sType == DUI_MSGTYPE_TABNOACTIVEFORM)
	{
		for (int i = 0; i < m_listForm.GetSize(); i++)
		{
			CUIForm* pForm = (CUIForm*)m_listForm.GetAt(i);
			if (pForm->IsForm(msg.pSender->GetName()))
			{
				pForm->OnHideForm();
				return;
			}
		}
	}

	for (int i = 0; i < m_listForm.GetSize(); i++)
	{
		CUIForm* pForm = (CUIForm*)m_listForm.GetAt(i);
		pForm->Notify(msg);
	}

	CUIFrmBase::Notify(msg);
}

LRESULT CUIFrameWndBase::MessageHandler(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, bool& bHandled)
{
	if (uMsg == WM_KEYDOWN)
	{
		switch (wParam)
		{
		case VK_RETURN:
		case VK_ESCAPE:
			{
				LRESULT lResult = ResponseDefaultKeyEvent(wParam);
				if(lResult == S_OK)
				{
					bHandled = true;
					return S_OK;
				}
			}
		default:
			break;
		}
	}
	return S_FALSE;
}

LRESULT CUIFrameWndBase::HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, uiBool& bHandled)
{
	//WM_DPICHANGED(跨屏 DPI 变化)已下沉到 CPaintManagerUI::MessageHandler 统一处理,
	//采用 lParam 建议矩形(#23-②);此处不再拦截。宿主如需感知时序,处理 UIMSG_SET_DPI 即可。

	if (uMsg == UIMSG_GRID_NOTIFY)
	{
		CGridUI* pGrid = (CGridUI*)wParam;
		GetManager()->SendNotify(pGrid, _T("CGridUI::OnGridNotify"), wParam, lParam);
		return 0;
	}

	if(uMsg == UIMSG_CREATE_MENU)
	{
		CDuiString *pstring = (CDuiString *)wParam;
		CreateMenu(pstring->GetData());
		delete pstring;
		return 0;
	}

	if(uMsg == UIMSG_CONTROL_ACTION)
	{
		TUIAction *act = (TUIAction *)wParam;
		ASSERT(act);
		UIAction(act, false);
		return 0;
	}

	if(uMsg == UIMSG_CONTROL_ACTION_ASYNC)
	{
		TUIAction *act = (TUIAction *)wParam;
		ASSERT(act);
		UIAction(act, true);
		delete act;
	}

	if(OnCustomMessage(uMsg, wParam, lParam))
	{
		return 0;
	}

	for (int i=0; i<m_listForm.GetSize(); i++)
	{
		CUIForm *pForm = (CUIForm *)m_listForm.GetAt(i);
		if(pForm->OnCustomMessage(uMsg, wParam, lParam))
		{
			bHandled = uiTrue;
			return 0;
		}
	}

	bHandled = uiFalse;
	return 0;
}

LRESULT CUIFrameWndBase::HandleMenuCommandMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, uiBool& bHandled)
{
	if(uMsg == UIMSG_MENUCLICK)
	{
		bHandled = uiTrue;

		MenuCmd* pMenuCmd = (MenuCmd*)wParam;
		if(pMenuCmd)
		{
			if(OnMenuCommand(pMenuCmd))
			{
				delete pMenuCmd;
				bHandled = uiTrue;
				return 0;
			}

			for (int i=0; i<m_listForm.GetSize(); i++)
			{
				CUIForm *pForm = (CUIForm *)m_listForm.GetAt(i);
				if(pForm->OnMenuCommand(pMenuCmd))
				{
					delete pMenuCmd;
					bHandled = uiTrue;
					return 0; 
				}
			}

			delete pMenuCmd;
			return 0;
		}
	}
	else if(uMsg == UIMSG_MENU_UPDATE_COMMAND_UI)
	{
		bHandled = uiTrue;

		CMenuCmdUI* pCmdUI = (CMenuCmdUI *)wParam;
		if(pCmdUI)
		{
			if(OnMenuUpdateCommandUI(pCmdUI))
			{
				bHandled = uiTrue;
				return 1;
			}

			for (int i=0; i<m_listForm.GetSize(); i++)
			{
				CUIForm *pForm = (CUIForm *)m_listForm.GetAt(i);
				if(pForm->OnMenuUpdateCommandUI(pCmdUI))
				{
					bHandled = uiTrue;
					return 1; 
				}
			}

			return 0;
		}
	}

	return 0;
}

void CUIFrameWndBase::__InitWindow()
{
	for (int i=0; i<m_listForm.GetSize(); i++)
	{
		CUIForm *pForm = (CUIForm *)m_listForm.GetAt(i);
		pForm->__InitWindow();
		pForm->InitWindow();
	}
	InitWindow();
}

void CUIFrameWndBase::UIAction(TUIAction *act, bool bAsync)
{
	CControlUI *pControl = GetManager()->FindControl(act->sControlName);
	if(!pControl) return;

	if (act->action == UIACTION_SetText)
	{
		pControl->SetText((LPCTSTR)act->wParam);
		return;
	}

	if (act->action == UIACTION_SetValue)
	{
		CProgressUI *pProgress = dynamic_cast<CProgressUI *>(pControl);
		if(pProgress)
		{
			pProgress->SetValue(act->wParam);
		}
		return;
	}

	if (act->action == UIACTION_SetMinValue)
	{
		CProgressUI *pProgress = dynamic_cast<CProgressUI *>(pControl);
		if(pProgress)
		{
			pProgress->SetMinValue(act->wParam);
		}
		return;
	}

	if (act->action == UIACTION_SetMaxValue)
	{
		CProgressUI *pProgress = dynamic_cast<CProgressUI *>(pControl);
		if(pProgress)
		{
			pProgress->SetMaxValue(act->wParam);
		}
		return;
	}

	if (act->action == UIACTION_SetGridCellText)
	{
		CGridUI *pGrid = dynamic_cast<CGridUI *>(pControl);
		if(pGrid)
		{
			TCellID *p = (TCellID *)act->lParam;
			TCellData *pCell = pGrid->GetCellData(p->row, p->col);
			if(pCell)
			{
				pCell->SetText((LPCTSTR)act->wParam);
			}
		}
		return;
	}

	if (act->action == UIACTION_SetGridCellTextColor)
	{
		CGridUI *pGrid = dynamic_cast<CGridUI *>(pControl);
		if(pGrid)
		{
			TCellID *p = (TCellID *)act->lParam;
			TCellData *pCell = pGrid->GetCellData(p->row, p->col);
			if(pCell)
			{
				pCell->SetTextColor((DWORD)act->wParam);
			}
		}
		return;
	}

	if (act->action == UIACTION_GridRefresh)
	{
		CGridUI *pGrid = dynamic_cast<CGridUI *>(pControl);
		if(pGrid)
		{
			uiBool bNeedUpdate = (uiBool)act->wParam;
			pGrid->Refresh(bNeedUpdate);
		}
		return;
	}

	if (act->action == UIACTION_SetVisible)
	{
		pControl->SetVisible((uiBool)act->wParam == uiTrue);
		return;
	}

	if(act->action == UIACTION_Close)
	{
		Close(act->wParam);
		return;
	}
}

uiBool CUIFrameWndBase::IsInStaticControl(CControlUI *pControl)
{
	uiBool bRet = uiFalse;
	if (!pControl)
	{
		return bRet;
	}

	CDuiString strClassName = pControl->GetClass();
	strClassName.MakeLower();

	std::vector<CDuiString>::iterator it = std::find(m_vctStaticName.begin(), m_vctStaticName.end(), strClassName);
	if (m_vctStaticName.end() != it)
	{
		CControlUI* pParent = pControl->GetParent();
		while (pParent)
		{
			strClassName = pParent->GetClass();
			strClassName.MakeLower();
			it = std::find(m_vctStaticName.begin(), m_vctStaticName.end(), strClassName);
			if (m_vctStaticName.end() == it)
			{
				return bRet;
			}

			pParent = pParent->GetParent();
		}

		bRet = uiTrue;
	}

	return bRet;
}

} //namespace DuiLib{
