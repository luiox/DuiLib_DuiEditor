#include "StdAfx.h"
#include "UIFile.h"

#include <stdarg.h>
#include <stdio.h>

// PathIsRelative 原经 UIlib.h→UIImageBoxEx.h→atlimage.h 传递引入（atlimage.h 内部
// #include <Shlwapi.h>）；DUILIB_NO_IMAGEBOXEX 门控下无 ATL，需显式包含
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#ifdef WIN32
#include "../Utils/unzip.cpp"
#endif
///////////////////////////////////////////////////////////////////////////////////////
namespace DuiLib {

	CUIFile::CUIFile()
	{
		m_pData = NULL; 
		m_dwSize = 0;
		m_fp = NULL;
		m_charset = CUIFile::FILE_ANSI;
	}

	CUIFile::~CUIFile()
	{
		Empty();
		Close();
	}

	LPBYTE CUIFile::GetData() const
	{
		return m_pData;
	}

	DWORD  CUIFile::GetSize() const
	{
		return m_dwSize;
	}

	void CUIFile::Empty()
	{
		if(m_pData != NULL) { delete []m_pData; m_pData = NULL; }
		m_dwSize = 0U;
	}

	uiBool CUIFile::LoadFile(const STRINGorID &bitmap, LPCTSTR type, HINSTANCE instance)
	{
		Empty();

		//读取顺序  zip文件 ==> skin文件夹 ==> 资源 ==> 文件绝对路径
		if (type == NULL)
		{
			if (__LoadFromZip(bitmap, type, instance))
				return uiTrue;
			if (__LoadFromSkinPath(bitmap, type, instance))
				return uiTrue;
		}
		else
		{
#ifdef DUILIB_WIN32
			if (__LoadFromResource(bitmap, type, instance))
				return uiTrue;
#endif
		}

		return __LoadFromDiskPath(bitmap.m_lpstr);
	}


	uiBool CUIFile::LoadFile(LPCTSTR pStrImage, LPCTSTR type, HINSTANCE instance)
	{
		return LoadFile(STRINGorID(pStrImage), type, instance);
	}

#ifdef WIN32
	uiBool CUIFile::LoadFile(UINT nID, LPCTSTR type, HINSTANCE instance)
	{
		return LoadFile(STRINGorID(nID), type, instance);
	}
#endif

	uiBool CUIFile::__LoadFromSkinPath(const STRINGorID& bitmap, LPCTSTR type, HINSTANCE instance)
	{
		CDuiString sFile = CPaintManagerUI::GetResourcePath();	
		sFile += bitmap.m_lpstr;
#ifndef DUILIB_WIN32
		sFile.Replace(_T("\\"), _T("/"));
#endif
		return __LoadFromDiskPath(sFile);
	}

	uiBool CUIFile::__LoadFromZip(const STRINGorID& bitmap, LPCTSTR type, HINSTANCE instance)
	{
		if (CPaintManagerUI::GetResourceZip().IsEmpty())
			return uiFalse;

#ifdef WIN32
		HZIP hz = NULL;
		do 
		{
			CDuiString sFile = CPaintManagerUI::GetResourcePath();
			sFile += CPaintManagerUI::GetResourceZip();
			if (CPaintManagerUI::IsCachedResourceZip())  //zip资源
			{
				hz = (HZIP)CPaintManagerUI::GetResourceZipHandle();
			}
			else //zip文件
			{
				CDuiString sFilePwd = CPaintManagerUI::GetResourceZipPwd();
				UISTRING_CONVERSION;
				hz = OpenZip(sFile.GetData(), UIT2A(sFilePwd.GetData()));
			}
			if (hz == NULL) break;
			ZIPENTRY ze;
			int i = 0;
			CDuiString key = bitmap.m_lpstr;
			key.Replace(_T("\\"), _T("/"));
			if (FindZipItem(hz, key, true, &i, &ze) != 0) break;
			m_dwSize = ze.unc_size;
			if (m_dwSize == 0) break;
			m_pData = new BYTE[m_dwSize + 1];
			m_pData[m_dwSize] = '\0';
			int res = UnzipItem(hz, i, m_pData, m_dwSize);
			if (res != 0x00000000 && res != 0x00000600)
			{
				Empty();
			}
		} while (0);

		if (!CPaintManagerUI::IsCachedResourceZip())
			CloseZip(hz);
#endif

		return m_dwSize > 0;
	}

#ifdef WIN32
	uiBool CUIFile::__LoadFromResource(const STRINGorID& bitmap, LPCTSTR type, HINSTANCE instance)
	{
		HINSTANCE dllinstance = NULL;
		if (instance)
		{
			dllinstance = instance;
		}
		else
		{
			dllinstance = CPaintManagerUI::GetResourceDll();
		}

		HRSRC hResource = ::FindResource(dllinstance, bitmap.m_lpstr, type);
		if (hResource == NULL) return uiFalse;
		HGLOBAL hGlobal = ::LoadResource(dllinstance, hResource);
		if (hGlobal == NULL)
		{
			FreeResource(hResource);
			return uiFalse;
		}

		m_dwSize = ::SizeofResource(dllinstance, hResource);
		if (m_dwSize == 0)
		{
			FreeResource(hResource);
			return uiFalse;
		}
		m_pData = new BYTE[m_dwSize + 1];
		m_pData[m_dwSize] = '\0';
		::CopyMemory(m_pData, (LPBYTE)::LockResource(hGlobal), m_dwSize);
		::FreeResource(hResource);

		return m_dwSize > 0;
	}
#endif

	uiBool CUIFile::__LoadFromDiskPath(LPCTSTR sFilePath)
	{
	#ifdef DUILIB_SDL
		CDuiStringUtf8 utf8Path(sFilePath);
		SDL_IOStream* io = SDL_IOFromFile(utf8Path.GetData(), "rb");
		if (!io) return uiFalse;

		Sint64 size = SDL_GetIOSize(io);
		if (size <= 0) {
			SDL_CloseIO(io);
			return uiFalse;
		}

		m_pData = new BYTE[(size_t)size + 1];
		m_pData[(size_t)size] = '\0';
		size_t readSize = SDL_ReadIO(io, m_pData, (size_t)size);
		SDL_CloseIO(io);

		if (readSize != (size_t)size) {
			Empty();
			return uiFalse;
		}
		m_dwSize = (DWORD)size;
		return uiTrue;
	#else
		//直接去读取bitmap.m_lpstr指向的路径
		FILE *file = _tfopen(sFilePath, _T("rb"));
		if(file==NULL) return uiFalse;
		if(feof(file)) return uiFalse;

		int pos = ftell(file);
		fseek(file, 0, SEEK_END);
		m_dwSize = ftell(file);
		fseek(file, pos, SEEK_SET);

		if (m_dwSize == 0)
		{
			fclose(file);
			return uiFalse;
		}

		m_pData = new BYTE[m_dwSize + 1];
		memset(m_pData, 0, sizeof(BYTE)*(m_dwSize+1));
		int nRead = fread(m_pData, 1, m_dwSize, file);
		fclose(file);
		return m_dwSize > 0;
	#endif
	}

	uiBool CUIFile::Open(LPCTSTR lpszFileName, LPCTSTR mode)
	{
	#ifdef DUILIB_SDL
		CDuiStringUtf8 utf8Path(lpszFileName);
		CDuiStringUtf8 utf8Mode(mode);
		m_fp = SDL_IOFromFile(utf8Path.GetData(), utf8Mode.GetData());
		return m_fp != NULL;
	#else
		m_fp = _tfopen(lpszFileName, mode);
		return m_fp != NULL;
	#endif
	}

	void CUIFile::Close()
	{
		if (m_fp) 
		{
		#ifdef DUILIB_SDL
			SDL_CloseIO(m_fp);
		#else
			fclose(m_fp);
		#endif
			m_fp = NULL;
		}
	}

	UINT CUIFile::Read(void* lpBuf, UINT nCount)
	{
	#ifdef DUILIB_SDL
		return (UINT)SDL_ReadIO(m_fp, lpBuf, nCount);
	#else
		return fread(lpBuf, 1, nCount, m_fp);
	#endif
	}

	UINT CUIFile::Write(const void* lpBuf, UINT nCount)
	{
		#ifdef DUILIB_SDL
		return (UINT)SDL_WriteIO(m_fp, lpBuf, nCount);
		#else
		return fwrite(lpBuf, 1, nCount, m_fp);
		#endif
	}

	UINT CUIFile::WriteV(const char* lpszFormat, ...)
	{
		UINT nWrite = 0;
		UISTRING_CONVERSION;
		CDuiString sFormat = UIA2T(lpszFormat);

		CDuiString strText;
		va_list argList;
		va_start(argList, lpszFormat);
		strText.InnerFormat(sFormat, argList);
		va_end(argList);

		if(m_charset == CUIFile::FILE_UTF8)
		{
			const char* pText = UIT2UTF8(strText);
			nWrite = Write(pText, strlen(pText));
		}
		else if(m_charset == CUIFile::FILE_ANSI)
		{
			const char* pText = UIT2A(strText);
			nWrite = Write(pText, strlen(pText));
		}
		return nWrite;
	}

	UINT CUIFile::WriteV(const wchar_t* lpszFormat, ...)
	{
		UINT nWrite = 0;
		UISTRING_CONVERSION;
		CDuiString sFormat = UIW2T(lpszFormat);

		CDuiString strText;
		va_list argList;
		va_start(argList, lpszFormat);
		strText.InnerFormat(sFormat, argList);
		va_end(argList);

		if(m_charset == CUIFile::FILE_UTF8)
		{
			const char* pText = UIT2UTF8(strText);
			nWrite = Write(pText, strlen(pText));
		}
		else if(m_charset == CUIFile::FILE_ANSI)
		{
			const char* pText = UIT2A(strText);
			nWrite = Write(pText, strlen(pText));
		}
		return nWrite;
	}

	UINT CUIFile::WriteLine(const char* lpszFormat, ...)
	{
		UINT nWrite = 0;
		UISTRING_CONVERSION;
		CDuiString sFormat = UIA2T(lpszFormat);

		CDuiString strText;
		va_list argList;
		va_start(argList, lpszFormat);
		strText.InnerFormat(sFormat, argList);
		va_end(argList);

		if (m_charset == CUIFile::FILE_UTF8)
		{
			const char* pText = UIT2UTF8(strText);
			nWrite = Write(pText, strlen(pText));
		}
		else if (m_charset == CUIFile::FILE_ANSI)
		{
			const char* pText = UIT2A(strText);
			nWrite = Write(pText, strlen(pText));
		}
		Write("\n", 1);
		return nWrite;
	}

	UINT CUIFile::WriteLine(const wchar_t* lpszFormat, ...)
	{
		UINT nWrite = 0;
		UISTRING_CONVERSION;
		CDuiString sFormat = UIW2T(lpszFormat);

		CDuiString strText;
		va_list argList;
		va_start(argList, lpszFormat);
		strText.InnerFormat(sFormat, argList);
		va_end(argList);

		if (m_charset == CUIFile::FILE_UTF8)
		{
			const char* pText = UIT2UTF8(strText);
			nWrite = Write(pText, strlen(pText));
		}
		else if (m_charset == CUIFile::FILE_ANSI)
		{
			const char* pText = UIT2A(strText);
			nWrite = Write(pText, strlen(pText));
		}
		Write("\n", 1);
		return nWrite;
	}

	UINT CUIFile::GetFileLength() const
	{
	#ifdef DUILIB_SDL
		if (!m_fp) return 0;
		return (UINT)SDL_GetIOSize(m_fp);
	#else
		DWORD fsize;
		int pos = ftell(m_fp);
		fseek(m_fp, 0, SEEK_END);
		fsize = ftell(m_fp);
		fseek(m_fp, pos, SEEK_SET);
		return fsize;
	#endif
	}

	int CUIFile::SeekToEnd()
	{
	#ifdef DUILIB_SDL
		return (SDL_SeekIO(m_fp, 0, SDL_IO_SEEK_END) < 0) ? -1 : 0;
	#else
		return fseek(m_fp, 0, SEEK_END);
	#endif
	}

	void CUIFile::SeekToBegin()
	{
	#ifdef DUILIB_SDL
		SDL_SeekIO(m_fp, 0, SDL_IO_SEEK_SET);
	#else
		fseek(m_fp, 0, SEEK_SET);
	#endif
	}

	int CUIFile::Seek(UINT lOff, int nFrom)
	{
	#ifdef DUILIB_SDL
		if (!m_fp) return -1;

		SDL_IOWhence origin;
		switch (nFrom) {
			case SEEK_SET: origin = SDL_IO_SEEK_SET; break;
			case SEEK_CUR: origin = SDL_IO_SEEK_CUR; break;
			case SEEK_END: origin = SDL_IO_SEEK_END; break;
			default: return -1;
		}

		// SDL_SeekIO 返回新偏移，失败返回 -1
		if (SDL_SeekIO(m_fp, lOff, origin) < 0) {
			SDL_Log("SDL_SeekIO failed: %s", SDL_GetError());
			return -1;
		}
		return 0;
	#else
		if (!m_fp) return -1;
		return (fseek(m_fp, lOff, nFrom) == 0) ? 0 : -1;
	#endif
	}

	uiBool CUIFile::IsEOF()
	{
	#ifdef DUILIB_SDL
		if (!m_fp) return uiTrue;
		return (SDL_GetIOStatus(m_fp) == SDL_IO_STATUS_EOF);
	#else
		return feof(m_fp) == 1;
	#endif
	}

	void CUIFile::SetCharSet(CUIFile::CharSet charset)
	{
		m_charset = charset;
	}

	//////////////////////////////////////////////////////////////////////////
	uiBool CUIFile::CopyFile(LPCTSTR sSourceFilePathName, LPCTSTR sDestFilePathName, uiBool bFailIfExists)
	{
#ifdef WIN32
		return ::CopyFile(sSourceFilePathName, sDestFilePathName, bFailIfExists);
#elif defined __linux__
		CDuiString s;
		s.Format(_T("cp \"%s\" \"%s\""), sSourceFilePathName, sDestFilePathName);
		system(s.GetData());
#elif defined __APPLE__
        CDuiString s;
        s.Format(_T("cp \"%s\" \"%s\""), sSourceFilePathName, sDestFilePathName);
        system(s.GetData());
#endif
		return uiFalse;
	}

	uiBool CUIFile::DeleteFile(LPCTSTR sFilePathName)
	{
#ifdef WIN32
		return ::DeleteFile(sFilePathName);
#elif defined __linux__
		//CDuiString s;
		//s.Format(_T("rm \"%s\""), sFilePathName);
		//system(s.GetData());
		return remove(sFilePathName) == 0;
#elif defined __APPLE__
        //CDuiString s;
        //s.Format(_T("rm \"%s\""), sFilePathName);
		//system(s.GetData());
		return remove(sFilePathName) == 0;
#endif
		return uiFalse;
	}

	uiBool CUIFile::IsFileExist(LPCTSTR sFilePathName)
	{
#ifdef WIN32
		DWORD dwAttrib = GetFileAttributes(sFilePathName);
		if( dwAttrib == INVALID_FILE_ATTRIBUTES) return uiFalse;
		return (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY;
#elif defined __linux__
		//return access(sPathName, F_OK) == 0;
		struct stat buff;
		return stat(sFilePathName, &buff) == 0 && S_ISREG(buff.st_mode);
#elif defined __APPLE__
        struct stat buff;
        return stat(sFilePathName, &buff) == 0 && S_ISREG(buff.st_mode);
#endif
		return uiFalse;
	}

	uiBool CUIFile::CreateDirectory(LPCTSTR sPathName, uiBool bCreateMultiLevelDirectory)
	{
		if (IsDirectoryExist(sPathName))
			return uiTrue;

		if(bCreateMultiLevelDirectory)
		{
			CDuiString sPath = sPathName;
			CDuiString temp;
			int len = sPath.GetLength();
			for (int i=0; i<sPath.GetLength(); i++)
			{
				TCHAR ch = sPath[i];
				temp += sPath[i];
				if (sPath[i] == _T('/') || sPath[i] == _T('\\') || i == sPath.GetLength()-1)
				{
					if (!temp.IsEmpty() && !IsDirectoryExist(temp))
					{
						if (!CreateDirectory(temp.GetData(), uiFalse))
							return uiFalse;
					}
				}
			}
		}
		else
		{
#ifdef WIN32
			return ::CreateDirectory(sPathName, NULL);
#elif defined __linux__
#ifdef _DEBUG
			int n = ::mkdir(sPathName, S_IRWXU | S_IRWXG | S_IRWXO);
			int err = errno;
			return n == 0;
#else
			return ::mkdir(sPathName, S_IRWXU | S_IRWXG | S_IRWXO) == 0;
#endif
#elif defined __APPLE__
            return ::mkdir(sPathName, S_IRWXU | S_IRWXG | S_IRWXO) == 0;
#endif
		}
		return uiTrue;
	}

	uiBool CUIFile::RemoveDirectory(LPCTSTR sPathName, uiBool bDeleteNoEmptyDirectory)
	{
		if (bDeleteNoEmptyDirectory)
		{
			if (!IsDirectoryExist(sPathName))
				return uiFalse;

			CUIFileFind finder;
			CDuiString sFindPath = sPathName;
			if(sFindPath.GetAt(sFindPath.GetLength()-1) != _T('\\') 
				|| sFindPath.GetAt(sFindPath.GetLength()-1) != _T('/'))
				sFindPath += _T("/");
#ifdef WIN32
			sFindPath += _T("*.*");
#endif
			uiBool bFind = finder.FindFile(sFindPath);
			while (bFind)
			{
				bFind = finder.FindNextFile();
				if (finder.IsDots())
					continue;

				CDuiString sFilePath = finder.GetFilePath();
				if (finder.IsDirectory())
				{
					RemoveDirectory(sFilePath, uiTrue);
					continue;
				}

				if (!DeleteFile(sFilePath))
				{
					return uiFalse;
				}
			}
			return RemoveDirectory(sPathName, uiFalse);
		}
		else
		{
#ifdef WIN32
			return ::RemoveDirectory(sPathName);
#elif defined __linux__
			return ::rmdir(sPathName) == 0;
#elif defined __APPLE__
			return ::rmdir(sPathName) == 0;
#endif
		}
		return uiFalse;
	}

	uiBool CUIFile::IsDirectoryExist(LPCTSTR sPathName)
	{
#ifdef WIN32
		DWORD dwAttrib = GetFileAttributes(sPathName);
		if( dwAttrib == INVALID_FILE_ATTRIBUTES) return uiFalse;
		return (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
#elif defined __linux__
		//return access(sPathName, F_OK) == 0;
		struct stat buff;
#ifdef _DEBUG
		int n = stat(sPathName, &buff);
		uiBool b = S_ISDIR(buff.st_mode);
		int error = errno;
		return n == 0 && b == uiTrue;
#else
		return stat(sPathName, &buff) == 0 && S_ISDIR(buff.st_mode);
#endif
#elif defined __APPLE__
        struct stat buff;
        return stat(sPathName, &buff) == 0 && S_ISDIR(buff.st_mode);
#endif
		return uiFalse;
	}

	//////////////////////////////////////////////////////////////////////////
	CUIFileFind::CUIFileFind()
	{
#ifdef WIN32
		m_hFind = INVALID_HANDLE_VALUE;
		m_pFoundInfo = NULL;
		m_pNextInfo = NULL;
#elif defined __linux__
		m_dir = NULL;
		m_dirFoundInfo = NULL;
		m_dirNextInfo = NULL;
		m_scandirList = NULL;
		m_scandirCount = 0;
		m_scandirIterator = 0;
#elif defined __APPLE__
        m_dir = NULL;
        m_dirFoundInfo = NULL;
        m_dirNextInfo = NULL;
        m_scandirList = NULL;
        m_scandirCount = 0;
        m_scandirIterator = 0;
#endif
	}

	CUIFileFind::~CUIFileFind()
	{
		FindClose();
	}

	uiBool CUIFileFind::FindFile(LPCTSTR lpFileName)
	{
		FindClose();

		if (lpFileName == NULL) 
			return uiFalse;

#ifdef DUILIB_WIN32
		//是否相对路径
		uiBool bRelativePath = PathIsRelative(lpFileName);
		if (bRelativePath)
		{		
			//在程序目录下搜索
			m_pNextInfo = new WIN32_FIND_DATA; 
			m_strRoot = CPaintManagerUI::GetInstancePath() + lpFileName;

			m_hFind = ::FindFirstFile(CPaintManagerUI::GetInstancePath() + lpFileName, (LPWIN32_FIND_DATA)m_pNextInfo);
			if (m_hFind != INVALID_HANDLE_VALUE)
				return uiTrue;
			delete m_pNextInfo;
			m_pNextInfo = NULL;
		}
		else
		{
			LPCTSTR pstr = _tfullpath(m_strRoot.GetBuffer(_MAX_PATH), lpFileName, _MAX_PATH);
			if (pstr != NULL)
			{
				TCHAR strDrive[_MAX_DRIVE], strDir[_MAX_DIR];
				TCHAR strPath[_MAX_PATH+1];					
				::_tsplitpath_s(m_strRoot.toString(), strDrive, _MAX_DRIVE, strDir, _MAX_DIR, NULL, 0, NULL, 0);
				::_tmakepath_s(strPath, _MAX_PATH, strDrive, strDir, NULL, NULL);
				m_strRoot = strPath;

				m_pNextInfo = new WIN32_FIND_DATA;
				m_hFind = ::FindFirstFile(lpFileName, (LPWIN32_FIND_DATA)m_pNextInfo);
				if (m_hFind != INVALID_HANDLE_VALUE)
					return uiTrue;
				delete m_pNextInfo;
				m_pNextInfo = NULL;
			}
		}
#elif defined __linux__
		m_strRoot = lpFileName;
		if (!CUIFile::IsDirectoryExist(lpFileName))
			return uiFalse;
		
// 		m_dir = opendir(lpFileName);
// 		if (NULL == m_dir)
// 		{
// 			return FALSE;
// 		}
// 		m_dirFoundInfo = readdir(m_dir);
// 		m_dirNextInfo = m_dirFoundInfo;
// 		return m_dirFoundInfo != NULL;

		m_scandirCount = scandir(lpFileName, &m_scandirList, 0, alphasort);
		if (m_scandirCount <= 0) return uiFalse;
		m_dirFoundInfo = m_scandirList[0];
		m_dirNextInfo = m_dirFoundInfo;
		m_scandirIterator = 0;
		return uiTrue;
#elif defined __APPLE__
        m_strRoot = lpFileName;
        if (!CUIFile::IsDirectoryExist(lpFileName))
            return uiFalse;
        
        m_scandirCount = scandir(lpFileName, &m_scandirList, 0, alphasort);
        if (m_scandirCount <= 0) return uiFalse;
        m_dirFoundInfo = m_scandirList[0];
        m_dirNextInfo = m_dirFoundInfo;
        m_scandirIterator = 0;
        return uiTrue;
#endif
		return uiFalse;
	}

	uiBool CUIFileFind::FindNextFile()
	{
#ifdef WIN32
		if (m_hFind == INVALID_HANDLE_VALUE)
			return uiFalse;

		if (m_pFoundInfo == NULL)
			m_pFoundInfo = new WIN32_FIND_DATA;

		void* pTemp = m_pFoundInfo;
		m_pFoundInfo = m_pNextInfo;
		m_pNextInfo = pTemp;
		return ::FindNextFile(m_hFind, (LPWIN32_FIND_DATA)m_pNextInfo);
#elif defined __linux__
// 		m_dirFoundInfo = m_dirNextInfo;
// 		m_dirNextInfo = readdir(m_dir);
// 		return m_dirNextInfo != NULL;
		m_dirFoundInfo = m_dirNextInfo;
		m_scandirIterator++;
		if (m_scandirIterator >= m_scandirCount)
			return uiFalse;
		m_dirNextInfo = m_scandirList[m_scandirIterator];
		return uiTrue;
#elif defined __APPLE__
        m_dirFoundInfo = m_dirNextInfo;
        m_scandirIterator++;
        if (m_scandirIterator >= m_scandirCount)
            return uiFalse;
        m_dirNextInfo = m_scandirList[m_scandirIterator];
        return uiTrue;
#endif
	}

	void CUIFileFind::FindClose()
	{
#ifdef WIN32
		if (m_pFoundInfo != NULL)
		{
			delete m_pFoundInfo;
			m_pFoundInfo = NULL;
		}

		if (m_pNextInfo != NULL)
		{
			delete m_pNextInfo;
			m_pNextInfo = NULL;
		}

		if (m_hFind != INVALID_HANDLE_VALUE) { 
			::FindClose(m_hFind); 
			m_hFind = INVALID_HANDLE_VALUE; 
		}
#elif defined __linux__
		if (m_dir != NULL)
		{
			closedir(m_dir);
			m_dir = NULL;
		}
		m_dirFoundInfo = NULL;
		m_dirNextInfo = NULL;
		if (m_scandirList != NULL)
		{
			free(m_scandirList);
			m_scandirCount = 0;
			m_scandirIterator = 0;
		}
#elif defined __APPLE__
        if (m_dir != NULL)
        {
            closedir(m_dir);
            m_dir = NULL;
        }
        m_dirFoundInfo = NULL;
        m_dirNextInfo = NULL;
        if (m_scandirList != NULL)
        {
            free(m_scandirList);
            m_scandirCount = 0;
            m_scandirIterator = 0;
        }
#endif
	}

	CDuiString CUIFileFind::GetFileName()
	{
		CDuiString sFile;
#ifdef WIN32
		if (m_pFoundInfo != NULL)
			sFile = ((LPWIN32_FIND_DATA)m_pFoundInfo)->cFileName;
#elif defined __linux__
		if(m_dirFoundInfo)
			sFile = m_dirFoundInfo->d_name;
#elif defined __APPLE__
        if(m_dirFoundInfo)
            sFile = m_dirFoundInfo->d_name;
#endif
		return sFile;
	}

	CDuiString CUIFileFind::GetFilePath()
	{
		CDuiString sFile;

#ifdef WIN32
		if (m_pFoundInfo != NULL)
		{
			sFile = m_strRoot;
			if(sFile.GetAt(sFile.GetLength()-1) != _T('\\') || sFile.GetAt(sFile.GetLength() - 1) != _T('/'))
				sFile += _T("\\");
			sFile += ((LPWIN32_FIND_DATA)m_pFoundInfo)->cFileName;
		}
#elif defined __linux__
		if (m_dirFoundInfo)
		{
			sFile = m_strRoot;
			if(sFile.GetAt(sFile.GetLength() - 1) != _T('/'))
				sFile += _T("/");
			sFile += m_dirFoundInfo->d_name;
		}
#elif defined __APPLE__
        if (m_dirFoundInfo)
        {
			sFile = m_strRoot;
			if(sFile.GetAt(sFile.GetLength() - 1) != _T('/'))
				sFile += _T("/");
            sFile += m_dirFoundInfo->d_name;
        }
#endif

		return sFile;
	}

// 	CDuiString CUIFileFind::GetFileTitleName()
// 	{
// 		CDuiString sFile;
// 		if (m_bFindInZip)
// 		{
// 			sFile = m_zipEntryNext.name;
// 		}
// 		else
// 		{
// 			if (m_pFoundInfo != NULL)
// 				sFile = ((LPWIN32_FIND_DATA)m_pFoundInfo)->cFileName;
// 		}
// 		return sFile;
// 	}
// 
// 	CDuiString CUIFileFind::GetFileExtName()
// 	{
// 		CDuiString sFile;
// 		if (m_bFindInZip)
// 		{
// 			sFile = m_zipEntryNext.name;
// 		}
// 		else
// 		{
// 			if (m_pFoundInfo != NULL)
// 				sFile = ((LPWIN32_FIND_DATA)m_pFoundInfo)->cFileName;
// 		}
// 		return sFile;
// 	}

	uiBool CUIFileFind::IsDots()
	{
		uiBool bResult = uiFalse;

#ifdef WIN32
		if (m_pFoundInfo != NULL && IsDirectory())
		{
			LPWIN32_FIND_DATA pFindData = (LPWIN32_FIND_DATA)m_pFoundInfo;
			if (pFindData->cFileName[0] == '.')
			{
				if (pFindData->cFileName[1] == '\0' ||
					(pFindData->cFileName[1] == '.' && pFindData->cFileName[2] == '\0'))
				{
					bResult = uiTrue;
				}
			}
		}
#elif defined __linux__
		if (m_dirFoundInfo != NULL && IsDirectory())
		{
			if (m_dirFoundInfo->d_name[0] == '.')
			{
				if (m_dirFoundInfo->d_name[1] == '\0' ||
					(m_dirFoundInfo->d_name[1] == '.' && m_dirFoundInfo->d_name[2] == '\0'))
				{
					bResult = uiTrue;
				}
			}
		}
#elif defined __APPLE__
        if (m_dirFoundInfo != NULL && IsDirectory())
        {
            if (m_dirFoundInfo->d_name[0] == '.')
            {
                if (m_dirFoundInfo->d_name[1] == '\0' ||
                    (m_dirFoundInfo->d_name[1] == '.' && m_dirFoundInfo->d_name[2] == '\0'))
                {
                    bResult = uiTrue;
                }
            }
        }
#endif

		return bResult;
	}

	uiBool CUIFileFind::IsDirectory()
	{
		uiBool bResult = uiFalse;

#ifdef WIN32
		if (m_pFoundInfo != NULL)
			bResult = (((LPWIN32_FIND_DATA)m_pFoundInfo)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
#elif defined __linux__
		if (m_dirFoundInfo)
		{
			bResult = m_dirFoundInfo->d_type == DT_DIR;
		}
#elif defined __APPLE__
        if (m_dirFoundInfo)
        {
            bResult = m_dirFoundInfo->d_type == DT_DIR;
        }
#endif

		return bResult;
	}

	void CUIFileFind::MakeFullPath(CDuiString &sPath)
	{
		//有 "*" 就去掉。
		int n = sPath.ReverseFind(_T('\\'));
		CDuiString sTemp = sPath.Right(sPath.GetLength()-n-1);
		if (sTemp.Find(_T('*') >= 0))
		{
			sPath = sPath.Left(n + 1);
		}
		return;
	}
	
} // namespace DuiLib


