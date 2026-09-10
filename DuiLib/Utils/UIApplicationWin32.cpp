#include "StdAfx.h"

#ifdef DUILIB_WIN32
// 注册表 API 所在库；ATL 门控构建下无 ATL 头的 defaultlib 指令兜底，需显式声明
#pragma comment(lib, "advapi32.lib")
namespace DuiLib{

CUIApplicationWin32::CUIApplicationWin32(void)
{
	m_hInstance = NULL;
	m_bCachedResourceZip = false;
	m_uZipResourceID = 0;

	m_bSingleApplication = false;
	m_hMutexApplication = NULL;
	m_UIAPP_SINGLEAPPLICATION_MSG = 0;
}

CUIApplicationWin32::~CUIApplicationWin32(void)
{
	
}

bool CUIApplicationWin32::InitInstance(HINSTANCE hInstance)
{
	// COM
	HRESULT Hr = ::CoInitialize(NULL);
	if( FAILED(Hr) ) return false;

	// OLE
	HRESULT hRes = ::OleInitialize(NULL);
	if( FAILED(hRes) ) return false;

	// 初始化UI管理器
	CPaintManagerUI::SetInstance(hInstance);
	m_strAppPath = CPaintManagerUI::GetInstancePath();

	// 初始化资源
	InitResource();

	// 是否单例运行程序
	if(m_bSingleApplication && (!m_GuidAppName.IsEmpty()) )
	{
		m_UIAPP_SINGLEAPPLICATION_MSG = ::RegisterWindowMessage(m_GuidAppName);
		m_hMutexApplication = CreateMutex(NULL, uiFalse, m_GuidAppName);
		if ( GetLastError() == ERROR_ALREADY_EXISTS )
		{
			DWORD dwRecipients = BSM_APPLICATIONS;
			::BroadcastSystemMessage( BSF_NOHANG, &dwRecipients, m_UIAPP_SINGLEAPPLICATION_MSG, 0, 0);
			return false;
		}
	}

	return true;
}

void CUIApplicationWin32::Run()
{
	if(m_pMainWnd)
	{
		m_pMainWnd->m_pApplication = this;
	}

	// 消息循环
	CPaintManagerWin32UI::MessageLoop();
}

int CUIApplicationWin32::ExitInstance()
{
	if(m_pMainWnd != NULL) { delete m_pMainWnd; m_pMainWnd = NULL; }

	if(m_hMutexApplication != NULL)
	{
		CloseHandle(m_hMutexApplication); m_hMutexApplication = NULL;
	}

	// 清理资源
	CPaintManagerUI::Term();
	// OLE
	OleUninitialize();
	// COM
	::CoUninitialize();

	return 0;
}




bool CUIApplicationWin32::SetSingleApplication(bool bSingle, LPCTSTR szGuidName) //只允许运行单一实例
{
	if(szGuidName == NULL)	return false;
	if(szGuidName[0] == '\0')	return false;

	m_bSingleApplication = bSingle;
	m_GuidAppName = szGuidName;
	return true;
}


void CUIApplicationWin32::SetRegistryKey(LPCTSTR lpszRegistryKey, LPCTSTR lpszAppName)
{
	RegistryKey.SetRegistryKey(lpszRegistryKey, lpszAppName);
}

UINT CUIApplicationWin32::GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault)
{
	return RegistryKey.ReadInt(lpszSection, lpszEntry, nDefault);
}

uiBool CUIApplicationWin32::WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue)
{
	return RegistryKey.WriteInt(lpszSection, lpszEntry, nValue);
}

CDuiString CUIApplicationWin32::GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault)
{
	return RegistryKey.ReadString(lpszSection, lpszEntry, lpszDefault);
}

uiBool CUIApplicationWin32::WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue)
{
	return RegistryKey.WriteString(lpszSection, lpszEntry, lpszValue);
}

uiBool CUIApplicationWin32::GetProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes)
{
	return RegistryKey.ReadBinary(lpszSection, lpszEntry, ppData, pBytes);
}

uiBool CUIApplicationWin32::WriteProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes)
{
	return RegistryKey.WriteBinary(lpszSection, lpszEntry, pData, nBytes);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CUIAppRegistryKey::CUIAppRegistryKey(void)
{
	m_pszRegistryKey = NULL;
	m_pszProfileName = NULL;
}

CUIAppRegistryKey::~CUIAppRegistryKey(void)
{
	free((void*)m_pszRegistryKey);
	free((void*)m_pszProfileName);
}


void CUIAppRegistryKey::SetRegistryKey(LPCTSTR lpszRegistryKey, LPCTSTR lpszAppName)
{
	ASSERT(m_pszRegistryKey == NULL);
	ASSERT(lpszAppName != NULL);

	free((void*)m_pszRegistryKey);
	m_pszRegistryKey = _tcsdup(lpszRegistryKey);
	free((void*)m_pszProfileName);
	m_pszProfileName = _tcsdup(lpszAppName);
}

HKEY CUIAppRegistryKey::GetAppRegistryKey()
{
	HKEY hAppKey = NULL;
	HKEY hSoftKey = NULL;
	HKEY hCompanyKey = NULL;

	LSTATUS lStatus = ::RegOpenKeyEx(HKEY_CURRENT_USER, _T("software"), 0, KEY_WRITE|KEY_READ, &hSoftKey);

	if (lStatus == ERROR_SUCCESS)
	{
		DWORD dw = 0;

		lStatus = ::RegCreateKeyEx(hSoftKey, m_pszRegistryKey, 0, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL, &hCompanyKey, &dw);
		if (lStatus == ERROR_SUCCESS)
		{
			lStatus = ::RegCreateKeyEx(hCompanyKey, m_pszProfileName, 0, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL, &hAppKey, &dw);
		}
	}
	if (hSoftKey != NULL)
		RegCloseKey(hSoftKey);
	if (hCompanyKey != NULL)
		RegCloseKey(hCompanyKey);

	return hAppKey;
}

HKEY CUIAppRegistryKey::GetSectionKey(LPCTSTR lpszSection)
{
	ASSERT(lpszSection != NULL);

	HKEY hSectionKey = NULL;
	HKEY hAppKey = GetAppRegistryKey();
	if (hAppKey == NULL)
		return NULL;

	DWORD dw;
	::RegCreateKeyEx(hAppKey, lpszSection, 0, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL, &hSectionKey, &dw);
	RegCloseKey(hAppKey);
	return hSectionKey;
}

UINT CUIAppRegistryKey::ReadInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault)
{
	ASSERT(lpszSection != NULL);
	ASSERT(lpszEntry != NULL);
	if (m_pszRegistryKey != NULL) // use registry
	{
		HKEY hSecKey = GetSectionKey(lpszSection);
		if (hSecKey == NULL)
			return nDefault;
		DWORD dwValue;
		DWORD dwType;
		DWORD dwCount = sizeof(DWORD);
		LONG lResult = RegQueryValueEx(hSecKey, (LPTSTR)lpszEntry, NULL, &dwType,
			(LPBYTE)&dwValue, &dwCount);
		RegCloseKey(hSecKey);
		if (lResult == ERROR_SUCCESS)
		{
			ASSERT(dwType == REG_DWORD);
			ASSERT(dwCount == sizeof(dwValue));
			return (UINT)dwValue;
		}
		return nDefault;
	}
	else
	{
		ASSERT(m_pszProfileName != NULL);
		return ::GetPrivateProfileInt(lpszSection, lpszEntry, nDefault, m_pszProfileName);
	}
}

uiBool CUIAppRegistryKey::WriteInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue)
{
	ASSERT(lpszSection != NULL);
	ASSERT(lpszEntry != NULL);
	if (m_pszRegistryKey != NULL)
	{
		HKEY hSecKey = GetSectionKey(lpszSection);
		if (hSecKey == NULL)
			return uiFalse;
		LONG lResult = RegSetValueEx(hSecKey, lpszEntry, NULL, REG_DWORD,
			(LPBYTE)&nValue, sizeof(nValue));
		RegCloseKey(hSecKey);
		return lResult == ERROR_SUCCESS;
	}
	else
	{
		ASSERT(m_pszProfileName != NULL);

		TCHAR szT[16];
		_stprintf_s(szT, _countof(szT), _T("%d"), nValue);
		return ::WritePrivateProfileString(lpszSection, lpszEntry, szT,
			m_pszProfileName);
	}
}

CDuiString CUIAppRegistryKey::ReadString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault)
{
	ASSERT(lpszSection != NULL);
	ASSERT(lpszEntry != NULL);
	if (m_pszRegistryKey != NULL)
	{
		HKEY hSecKey = GetSectionKey(lpszSection);
		if (hSecKey == NULL)
			return lpszDefault;
		// 原 ATL CString（经 UIImageBoxEx.h→atlimage.h 传递可见）；DUILIB_NO_IMAGEBOXEX
		// 门控下无 ATL，改用 CDuiString（REG_SZ 以 \0 结尾，按 \0 扫描与 ReleaseBuffer 等价）
		CDuiString strValue;
		DWORD dwType=REG_NONE;
		DWORD dwCount=0;
		LONG lResult = RegQueryValueEx(hSecKey, (LPTSTR)lpszEntry, NULL, &dwType,
			NULL, &dwCount);
		if (lResult == ERROR_SUCCESS)
		{
			ASSERT(dwType == REG_SZ);
			lResult = RegQueryValueEx(hSecKey, (LPTSTR)lpszEntry, NULL, &dwType,
				(LPBYTE)strValue.GetBuffer(dwCount/sizeof(TCHAR)), &dwCount);
		}
		RegCloseKey(hSecKey);
		if (lResult == ERROR_SUCCESS)
		{
			ASSERT(dwType == REG_SZ);
			return (LPCTSTR)strValue;
		}
		return lpszDefault;
	}
	else
	{
		ASSERT(m_pszProfileName != NULL);

		if (lpszDefault == NULL)
			lpszDefault = _T("");	// don't pass in NULL
		TCHAR szT[4096];
		DWORD dw = ::GetPrivateProfileString(lpszSection, lpszEntry,
			lpszDefault, szT, _countof(szT), m_pszProfileName);
		ASSERT(dw < 4095);
		return szT;
	}
}

uiBool CUIAppRegistryKey::WriteString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue)
{
	ASSERT(lpszSection != NULL);
	if (m_pszRegistryKey != NULL)
	{
		LONG lResult;
		if (lpszEntry == NULL) //delete whole section
		{
			HKEY hAppKey = GetAppRegistryKey();
			if (hAppKey == NULL)
				return uiFalse;
			lResult = ::RegDeleteKey(hAppKey, lpszSection);
			RegCloseKey(hAppKey);
		}
		else if (lpszValue == NULL)
		{
			HKEY hSecKey = GetSectionKey(lpszSection);
			if (hSecKey == NULL)
				return uiFalse;
			// necessary to cast away const below
			lResult = ::RegDeleteValue(hSecKey, (LPTSTR)lpszEntry);
			RegCloseKey(hSecKey);
		}
		else
		{
			HKEY hSecKey = GetSectionKey(lpszSection);
			if (hSecKey == NULL)
				return uiFalse;
			lResult = RegSetValueEx(hSecKey, lpszEntry, NULL, REG_SZ,
				(LPBYTE)lpszValue, (lstrlen(lpszValue)+1)*sizeof(TCHAR));
			RegCloseKey(hSecKey);
		}
		return lResult == ERROR_SUCCESS;
	}
	else
	{
		ASSERT(m_pszProfileName != NULL);
		ASSERT(lstrlen(m_pszProfileName) < 4095); // can't read in bigger
		return ::WritePrivateProfileString(lpszSection, lpszEntry, lpszValue,
			m_pszProfileName);
	}
}

uiBool CUIAppRegistryKey::ReadBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes)
{
	ASSERT(lpszSection != NULL);
	ASSERT(lpszEntry != NULL);
	ASSERT(ppData != NULL);
	ASSERT(pBytes != NULL);
	*ppData = NULL;
	*pBytes = 0;
	if (m_pszRegistryKey != NULL)
	{
		HKEY hSecKey = GetSectionKey(lpszSection);
		if (hSecKey == NULL)
		{
			return uiFalse;
		}

		// 原以 ::ATL::CRegKey rkSecKey(hSecKey) RAII 关闭句柄；去 ATL 后改为显式关闭
		DWORD dwType=0;
		DWORD dwCount=0;
		LONG lResult = RegQueryValueEx(hSecKey, (LPTSTR)lpszEntry, NULL, &dwType, NULL, &dwCount);
		*pBytes = dwCount;
		if (lResult == ERROR_SUCCESS)
		{
			ASSERT(dwType == REG_BINARY);
			*ppData = new BYTE[*pBytes];
			lResult = RegQueryValueEx(hSecKey, (LPTSTR)lpszEntry, NULL, &dwType,
				*ppData, &dwCount);
		}
		RegCloseKey(hSecKey);
		if (lResult == ERROR_SUCCESS)
		{
			ASSERT(dwType == REG_BINARY);
			return uiTrue;
		}
		else
		{
			delete [] *ppData;
			*ppData = NULL;
		}
		return uiFalse;
	}
	else
	{
		ASSERT(m_pszProfileName != NULL);

		CDuiString str = ReadString(lpszSection, lpszEntry, NULL);
		if (str.IsEmpty())
			return uiFalse;
		ASSERT(str.GetLength()%2 == 0);
		INT_PTR nLen = str.GetLength();
		*pBytes = UINT(nLen)/2;
		*ppData = new BYTE[*pBytes];
		for (int i=0;i<nLen;i+=2)
		{
			(*ppData)[i/2] = (BYTE)
				(((str[i+1] - 'A') << 4) + (str[i] - 'A'));
		}
		return uiTrue;
	}
}

uiBool CUIAppRegistryKey::WriteBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes)
{
	ASSERT(lpszSection != NULL);
	if (m_pszRegistryKey != NULL)
	{
		LONG lResult;
		HKEY hSecKey = GetSectionKey(lpszSection);
		if (hSecKey == NULL)
			return uiFalse;
		lResult = RegSetValueEx(hSecKey, lpszEntry, NULL, REG_BINARY,
			pData, nBytes);
		RegCloseKey(hSecKey);
		return lResult == ERROR_SUCCESS;
	}

	// convert to string and write out
	LPTSTR lpsz = new TCHAR[nBytes*2+1];
	UINT i;
	for (i = 0; i < nBytes; i++)
	{
		lpsz[i*2] = (TCHAR)((pData[i] & 0x0F) + 'A'); //low nibble
		lpsz[i*2+1] = (TCHAR)(((pData[i] >> 4) & 0x0F) + 'A'); //high nibble
	}
	lpsz[i*2] = 0;

	ASSERT(m_pszProfileName != NULL);

	uiBool bResult = WriteString(lpszSection, lpszEntry, lpsz);
	delete[] lpsz;
	return bResult;
}

uiBool CUIAppRegistryKey::GetValue(LPCTSTR lpszSection, LPCTSTR lpszEntry, int &nRet, int nDefault)
{
	ASSERT(lpszSection != NULL);
	ASSERT(lpszEntry != NULL);
	if (m_pszRegistryKey != NULL) // use registry
	{
		HKEY hSecKey = GetSectionKey(lpszSection);
		if (hSecKey == NULL)
		{
			nRet = nDefault;
			return uiFalse;
		}
		DWORD dwValue;
		DWORD dwType;
		DWORD dwCount = sizeof(DWORD);
		LONG lResult = RegQueryValueEx(hSecKey, (LPTSTR)lpszEntry, NULL, &dwType,
			(LPBYTE)&dwValue, &dwCount);
		RegCloseKey(hSecKey);
		if (lResult == ERROR_SUCCESS)
		{
			ASSERT(dwType == REG_DWORD);
			ASSERT(dwCount == sizeof(dwValue));
			nRet = (int)dwValue;
			return uiTrue;
		}
		nRet = nDefault;
		return uiFalse;
	}
	else
	{
		ASSERT(m_pszProfileName != NULL);
		nRet = ::GetPrivateProfileInt(lpszSection, lpszEntry, nDefault, m_pszProfileName);
		return uiTrue;
	}

	return uiFalse;
}

uiBool CUIAppRegistryKey::SetValue(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue)
{
	return WriteInt(lpszSection, lpszEntry, nValue);
}

uiBool CUIAppRegistryKey::GetValue(LPCTSTR lpszSection, LPCTSTR lpszEntry, CDuiString &strRet, LPCTSTR lpszDefault)
{
	ASSERT(lpszSection != NULL);
	ASSERT(lpszEntry != NULL);
	if (m_pszRegistryKey != NULL)
	{
		HKEY hSecKey = GetSectionKey(lpszSection);
		if (hSecKey == NULL)
		{
			strRet = lpszDefault;
			return uiFalse;
		}
		// 原 ATL CString（同 ReadString 处说明），改用 CDuiString
		CDuiString strValue;
		DWORD dwType=REG_NONE;
		DWORD dwCount=0;
		LONG lResult = RegQueryValueEx(hSecKey, (LPTSTR)lpszEntry, NULL, &dwType,
			NULL, &dwCount);
		if (lResult == ERROR_SUCCESS)
		{
			ASSERT(dwType == REG_SZ);
			lResult = RegQueryValueEx(hSecKey, (LPTSTR)lpszEntry, NULL, &dwType,
				(LPBYTE)strValue.GetBuffer(dwCount/sizeof(TCHAR)), &dwCount);
		}
		RegCloseKey(hSecKey);
		if (lResult == ERROR_SUCCESS)
		{
			ASSERT(dwType == REG_SZ);
			strRet = (LPCTSTR)strValue;
			return uiTrue;
		}
		strRet = lpszDefault;
		return uiFalse;
	}
	else
	{
		ASSERT(m_pszProfileName != NULL);

		if (lpszDefault == NULL)
			lpszDefault = _T("");	// don't pass in NULL
		TCHAR szT[4096];
		DWORD dw = ::GetPrivateProfileString(lpszSection, lpszEntry,
			lpszDefault, szT, _countof(szT), m_pszProfileName);
		ASSERT(dw < 4095);
		strRet = szT;
		return uiTrue;
	}

	return uiFalse;
}

uiBool CUIAppRegistryKey::SetValue(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR strValue)
{
	return WriteString(lpszSection, lpszEntry, strValue);
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CUIAppConfig::CUIAppConfig()
{

}

CUIAppConfig::~CUIAppConfig()
{

}

CXmlNodeUI CUIAppConfig::GetRoot()
{
	return m_xml.root().child_auto(UIGetApp()->GetAppName());
}

void CUIAppConfig::LoadConfig(LPCTSTR szPathName)
{
	if(szPathName == NULL || *szPathName == '\0')
		m_xml.load_file(UIGetApp()->GetAppPath() + UIGetApp()->GetAppName() + _T(".xml"));
	else
		m_xml.load_file(szPathName);
}

void CUIAppConfig::SaveConfig()
{
	m_xml.save_to_default_file();
}

uiBool CUIAppConfig::SaveControl(CXmlNodeUI nodeParent, CEditUI *pControl)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI node = nodeParent.child_auto(pControl->GetName());
	node.attribute_auto(_T("text")).set_value(pControl->GetText());
	return uiTrue;
}

uiBool CUIAppConfig::LoadControl(CXmlNodeUI nodeParent, CEditUI *pControl, LPCTSTR szDefault)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI node = nodeParent.child(pControl->GetName());
	pControl->SetText(node.attribute(_T("text")).as_string(szDefault));
	return uiTrue;
}

uiBool CUIAppConfig::SaveControl(CXmlNodeUI nodeParent, CRichEditUI *pControl)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI node = nodeParent.child_auto(pControl->GetName());
	node.attribute_auto(_T("text")).set_value(pControl->GetText());
	return uiTrue;
}

uiBool CUIAppConfig::LoadControl(CXmlNodeUI nodeParent, CRichEditUI *pControl, LPCTSTR szDefault)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI node = nodeParent.child(pControl->GetName());
	pControl->SetText(node.attribute(_T("text")).as_string(szDefault));
	return uiTrue;
}

uiBool CUIAppConfig::SaveControl(CXmlNodeUI nodeParent, CComboUI *pControl, uiBool bSaveItems)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI nodeCombo = nodeParent.child_auto(pControl->GetName());
	nodeCombo.attribute_auto(_T("text")).set_value(pControl->GetText());

	CXmlNodeUI nodeItem = nodeCombo.child_auto(_T("Item"));
	while(nodeItem.first_child()) { nodeItem.remove_child(nodeItem.first_child()); }

	for (int i=0; i<pControl->GetCount(); i++)
	{
		CControlUI *pItem = pControl->GetItemAt(i);
		CXmlNodeUI node = nodeItem.append_child(_T("Item"));
		node.attribute_auto(_T("text")).set_value(pItem->GetText());
	}
	return uiTrue;
}

uiBool CUIAppConfig::LoadControl(CXmlNodeUI nodeParent, CComboUI *pControl, LPCTSTR szDefault, uiBool bLoadItems)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI nodeCombo = nodeParent.child_auto(pControl->GetName());
	pControl->SetText(nodeCombo.attribute_auto(_T("text")).as_string(szDefault));

	CComboExUI *pCombo = (CComboExUI *)pControl;
	CXmlNodeUI nodeItem = nodeCombo.child(_T("Item"));
	for (CXmlNodeUI node=nodeItem.child(_T("Item")); node; node=node.next_sibling(_T("Item")))
	{
		pCombo->AddString(node.attribute_auto(_T("text")).as_string());
	}
	return uiTrue;
}

uiBool CUIAppConfig::SaveControl(CXmlNodeUI nodeParent, CComboExUI *pControl, uiBool bSaveItems)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI nodeCombo = nodeParent.child_auto(pControl->GetName());
	nodeCombo.attribute_auto(_T("text")).set_value(pControl->GetText());

	if(bSaveItems)
	{
		CXmlNodeUI nodeItem = nodeCombo.child_auto(_T("Item"));
		while(nodeItem.first_child()) { nodeItem.remove_child(nodeItem.first_child()); }

		for (int i=0; i<pControl->GetCount(); i++)
		{
			CControlUI *pItem = pControl->GetItemAt(i);
			CXmlNodeUI node = nodeItem.append_child(_T("Item"));
			node.attribute_auto(_T("text")).set_value(pItem->GetText());
		}
	}
	return uiTrue;
}

uiBool CUIAppConfig::LoadControl(CXmlNodeUI nodeParent, CComboExUI *pControl, LPCTSTR szDefault, uiBool bLoadItems)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI nodeCombo = nodeParent.child_auto(pControl->GetName());
	pControl->SetText(nodeCombo.attribute_auto(_T("text")).as_string(szDefault));

	if(bLoadItems)
	{
		CComboExUI *pCombo = (CComboExUI *)pControl;
		CXmlNodeUI nodeItem = nodeCombo.child(_T("Item"));
		for (CXmlNodeUI node=nodeItem.child(_T("Item")); node; node=node.next_sibling(_T("Item")))
		{
			pCombo->AddString(node.attribute_auto(_T("text")).as_string());
		}
	}
	return uiTrue;
}

uiBool CUIAppConfig::SaveControl(CXmlNodeUI nodeParent, COptionUI *pControl)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI node = nodeParent.child_auto(pControl->GetName());
	node.attribute_auto(_T("select")).set_value(pControl->IsSelected());
	return uiTrue;
}

uiBool CUIAppConfig::LoadControl(CXmlNodeUI nodeParent, COptionUI *pControl, uiBool bDefault)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI node = nodeParent.child(pControl->GetName());
	pControl->Selected(node.attribute(_T("select")).as_bool(bDefault == uiTrue), false);
	return uiTrue;
}


uiBool CUIAppConfig::SaveControl(CXmlNodeUI nodeParent, CTabLayoutUI *pControl)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI node = nodeParent.child_auto(pControl->GetName());
	node.attribute_auto(_T("sel")).set_value(pControl->GetCurSel());
	return uiTrue;
}

uiBool CUIAppConfig::LoadControl(CXmlNodeUI nodeParent, CTabLayoutUI *pControl, int nDefault)
{
	if(pControl->GetName().IsEmpty())
		return uiFalse;
	CXmlNodeUI node = nodeParent.child(pControl->GetName());
	pControl->SelectItem(node.attribute(_T("sel")).as_int(nDefault));
	return uiTrue;
}

} //namespace DuiLib{
#endif //#ifdef DUILIB_WIN32
