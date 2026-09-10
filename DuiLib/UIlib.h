
//#define UILIB_STATIC //静态库使用请在IDE添加预处理 UILIB_STATIC

#ifndef DUILIB_SDL
#define DUILIB_WIN32 //默认的
#endif

#if defined(__ANDROID__)
#define DUILIB_ANDROID
#elif defined(__linux__)
#define DUILIB_LINUX
#elif defined(__APPLE__)
#define DUILIB_MACOS
#endif

#include "compat.h"

#ifdef DUILIB_WIN32
	typedef HWND UIWND;
	#define DuiLibWindowWnd	CWindowWin32
	#define CShadowUI	CShadowWin32UI
	#define CDPI		CDpiWin32
	#define UIClip		UIClipWin32
	#define DuiLibPaintManagerUI CPaintManagerWin32UI
	#define CUIApplication CUIApplicationWin32
	#define CUIFrameWnd CUIFrameWndWin32
	#define DuiLibEditWnd CEditWndWin32
	#define CComboWnd CComboWndWin32
	#define CDateTimeWnd CDateTimeWndWin32
	#define CMenuWnd CMenuWndWin32
	#define CIPAddressWnd CIPAddressWndWin32
#elif defined DUILIB_SDL
	struct SDL_Window;
	struct SDL_Color;
	struct SDL_FColor;
	struct SDL_FRect;
	struct SDL_Rect;
	struct SDL_Mutex; 
	struct SDL_IOStream;
	typedef struct SDL_Window* UIWND;
	#define DuiLibWindowWnd	CWindowSDL
	#define CShadowUI CShadowSDLUI
	#define CDPI CDpiSDL
	#define UIClip UIClipSDL
	#define DuiLibPaintManagerUI CPaintManagerSDLUI
	#define CUIApplication CUIApplicationSDL
	#define CUIFrameWnd CUIFrameWndSDL
	#define DuiLibEditWnd CEditWndSDL
	#define CComboWnd CComboWndSDL
	#define CDateTimeWnd CDateTimeWndSDL
	#define CMenuWnd CMenuWndSDL
	#define CIPAddressWnd CIPAddressWndSDL
#endif //#ifdef DUILIB_WIN32

#include "Utils/DuiString.h"
#include "Utils/Md5.h"
#include "Utils/Utils.h"
#ifdef WIN32
#include "Utils/unzip.h"
#include "Utils/VersionHelpers.h"
#endif //#ifdef DUILIB_WIN32
#include "Core/UIFile.h"
#include "Core/UIXmlAttribute.h"
#include "Core/UIXmlNode.h"
#include "Core/UIXmlDocument.h"
#include "Utils/observer_impl_base.h"
#include "Utils/UIDelegate.h"
#include "Utils/TrayIcon.h"
#include "Utils/DPI.h"
#include "Utils/DpiWin32.h"
#include "Utils/DpiSdl.h"

#include "Core/UIDefine.h"
#include "Utils/UIShadowBase.h"
#include "Utils/UIShadowWin32.h"
#include "Utils/UIShadowSdl.h"

#include "Core/UIResourceManager.h"
#include "Core/UILangManager.h"
#include "Render/IRender.h"
#include "Core/UIScript.h"
#include "Core/UIManager.h"
#include "Core/UIManagerWin32.h"
#include "Core/UIManagerSDL.h"

#include "Core/UIBase.h"
#include "Core/UIWindowWin32.h"
#include "Core/UIWindowSDL.h"

#include "Core/ControlFactory.h"

#include "Control/UIAnimation.h"
#include "Core/UIControl.h"
#include "Core/UIContainer.h"

#include "Core/UIDlgBuilder.h"
#include "Utils/WinImplBase.h"

#include "Layout/UIVerticalLayout.h"
#include "Layout/UIHorizontalLayout.h"
#include "Layout/UITileLayout.h"
#include "Layout/UIDynamicLayout.h"
#include "Layout/UITabLayout.h"
#include "Layout/UIChildLayout.h"


#include "Control/UILabel.h"
#include "Control/UIText.h"

#include "Control/UIEdit.h"

#include "Control/UIGifAnim.h"


#include "Layout/UIAnimationTabLayout.h"
#include "Control/UIButton.h"
#include "Control/UIOption.h"
#include "Control/UITabCtrl.h"

#include "Control/UIList.h"
#include "Control/UICombo.h"
#include "Control/UIScrollBar.h"
#include "Control/UITreeView.h"

#include "Control/UIProgress.h"
#include "Control/UISlider.h"

#include "Control/UIRichEdit.h"
#include "Control/UIDateTime.h"
#include "Control/UIIPAddress.h"
#include "Control/UIIPAddressEx.h"

#include "Control/UIActiveX.h"
// ATL 相关控件门控：定义 DUILIB_NO_ACTIVEX 可在未安装 ATL 组件的环境编译
#ifndef DUILIB_NO_ACTIVEX
#include "Control/UIWebBrowser.h"
#include "Control/UIFlash.h"
#endif

#include "Control/UIMenu.h"
#include "Control/UIMenuWndWin32.h"
#include "Control/UIMenuWndSdl.h"

#include "Control/UIGroupBox.h"
#include "Control/UIRollText.h"
#include "Control/UIColorPalette.h"
#include "Control/UIListEx.h"
#include "Control/UIHotKey.h"
#include "Control/UIFadeButton.h"
#include "Control/UIRing.h"

#define UIMAKEIPADDRESS(b1,b2,b3,b4)  ((LPARAM)(((DWORD)(b1)<<24)+((DWORD)(b2)<<16)+((DWORD)(b3)<<8)+((DWORD)(b4))))
#define UIGETIP_B1(ip)  ((BYTE)((ip) >> 24))
#define UIGETIP_B2(ip)  ((BYTE)((ip) >> 16))
#define UIGETIP_B3(ip)  ((BYTE)((ip) >> 8))
#define UIGETIP_B4(ip)  ((BYTE)(ip))

#include "Control/UIIconButton.h"
#include "Control/UIDateTimeEx.h"
#include "Control/UIComboEx.h"
// ATL(atlimage.h) 门控：定义 DUILIB_NO_IMAGEBOXEX 可在未安装 ATL 组件的环境编译
#ifndef DUILIB_NO_IMAGEBOXEX
#include "Control/UIImageBoxEx.h"
#endif
#include "Control/UIRollTextEx.h"
#include "Control/UIMsgWnd.h"
#include "Control/UIPicture.h"
#include "Control/UIPictureBox.h"
#include "Control/UIQRCode.h"
#include "Control/UIBarCode.h"
#include "Control/UIGrid.h"
#include "Control/ITreeUI.h"
#include "Control/UITree.h"
#include "Control/UITreeItem.h"
#include "Control/UITracker.h"
#include "Control/UISpin.h"
#include "Layout/UITableLayout.h"
#include "Layout/UIChildWindow.h"

#include "Utils/UIApplication.h"
#include "Utils/UIApplicationWin32.h"
#include "Utils/UIApplicationSdl.h"

#include "Utils/UIFrameBase.h"
#include "Utils/UIFrameWnd.h"

#include "Utils/UIFrameWndWin32.h"
#include "Utils/UIFrameWndSdl.h"

#include "Utils/UIForm.h"
#include "Utils/UIDialog.h"

#include "Core/UIGlobal.h"

