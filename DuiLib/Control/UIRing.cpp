#include "StdAfx.h"
#include "UIRing.h"

#ifdef DUILIB_WIN32
namespace DuiLib
{
	IMPLEMENT_DUICONTROL(CRingUI)

	CRingUI::CRingUI() : m_fCurAngle(0.0f), m_pBkimage(NULL)
	{
	}

	CRingUI::~CRingUI()
	{
		if(m_pManager) m_pManager->KillTimer(this, RING_TIMERID);

		DeleteImage();
	}

	LPCTSTR CRingUI::GetClass() const
	{
		return _T("RingUI");
	}

	LPVOID CRingUI::GetInterface( LPCTSTR pstrName )
	{
		if( _tcscmp(pstrName, _T("Ring")) == 0 ) return static_cast<CRingUI*>(this);
		return CLabelUI::GetInterface(pstrName);
	}

	void CRingUI::SetAttribute( LPCTSTR pstrName, LPCTSTR pstrValue )
	{
		if( _tcscmp(pstrName, _T("bkimage")) == 0 ) SetBkImage(pstrValue);
		else CLabelUI::SetAttribute(pstrName, pstrValue);
	}

	void CRingUI::SetBkImage( LPCTSTR pStrImage )
	{
		if (m_sBkImage == pStrImage) return;
		m_sBkImage = pStrImage;
		DeleteImage();
		Invalidate();
	}

	void CRingUI::PaintBkImage( UIRender *pRender )
	{
		if(m_pBkimage == NULL) {
			InitImage();
		}

		if(m_pBkimage != NULL) {
			RECT rcItem = m_rcItem;
			int iWidth = rcItem.right - rcItem.left;
			int iHeight = rcItem.bottom - rcItem.top;
			Gdiplus::PointF centerPos(rcItem.left + iWidth/2, rcItem.top + iHeight/2);

			Gdiplus::Graphics graphics(pRender->GetDC());
			graphics.TranslateTransform(centerPos.X,centerPos.Y);
			graphics.RotateTransform(m_fCurAngle);
			graphics.TranslateTransform(-centerPos.X, -centerPos.Y);//��ԭԴ��
			graphics.DrawImage(m_pBkimage,rcItem.left,rcItem.top,iWidth,iHeight);
		}
	}

	void CRingUI::DoEvent( TEventUI& event )
	{
		if( event.Type == UIEVENT_TIMER && event.wParam == RING_TIMERID ) {
			if(m_fCurAngle > 359) {
				m_fCurAngle = 0;
			}
			m_fCurAngle += 36.0;
			Invalidate();
		}
		else {
			CLabelUI::DoEvent(event);
		}
	}

	void CRingUI::InitImage()
	{
		m_pBkimage = GdiplusLoadImage(GetBkImage());
		if ( NULL == m_pBkimage ) return;
		if(m_pManager) m_pManager->SetTimer(this, RING_TIMERID, 100);
	}

	void CRingUI::DeleteImage()
	{
		if ( m_pBkimage != NULL )
		{
			delete m_pBkimage;
			m_pBkimage = NULL;
		}
	}

	Gdiplus::Image*	CRingUI::GdiplusLoadImage(LPCTSTR pstrPath)
	{
		CUIFile f;
		if(!f.LoadFile(pstrPath)) 
			return NULL;

		HGLOBAL hMem = ::GlobalAlloc(GMEM_FIXED, f.GetSize());
		BYTE* pMem = (BYTE*)::GlobalLock(hMem);
		memcpy(pMem, f.GetData(), f.GetSize());
		IStream* pStm = NULL;
		::CreateStreamOnHGlobal(hMem, TRUE, &pStm);
		Gdiplus::Image *pImg = Gdiplus::Image::FromStream(pStm);
		if(!pImg || pImg->GetLastStatus() != Gdiplus::Ok)
		{
			pStm->Release();
			::GlobalUnlock(hMem);
			return 0;
		}
		pStm->Release();
		::GlobalUnlock(hMem);
		return pImg;
	}
}
#endif //#ifdef DUILIB_WIN32


