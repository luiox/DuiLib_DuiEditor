// tests/test_main.cpp —— DuiLib fork 最小测试基建（KNOWN_ISSUES #18）
//
// 轻量断言，零外部依赖（不引 gtest/check）。随根 xmake.lua 的
// target("duilib_test") 构建，set_default(false) 不影响默认构建；
// 运行：xmake run duilib_test（或 xmake test）。
//
// 覆盖方向（对应 3rd 提取收口）：
//   1-2. pugixml 独立编译单元（3rd/pugixml，见 3rd/README.md）——
//        本 TU 只带声明，实现经 DuiLib → pugixml target 链入，
//        链接/运行失败即回归（含 PUGIXML_WCHAR_MODE ABI 一致性）；
//   3-4. 核心工具件 CDuiString / CDuiRect（含 ATL 门控触点
//        CDuiRect(int,int,int,int)，见 KNOWN_ISSUES #20）；
//   5.   控件工厂（ATL 门控下注册集合变化的行为面）；
//   6.   nanosvg 解析（含原 tests/test_invariant_nanosvg.h 的
//        gradient-stops 不变量移植）；
//   7.   stb PNG 写入/读回内存回路（实现位于 Render/IRender.cpp 单 TU）；
//   8.   DPI 缩放/回缩往返不变量（CDpiBase，scale>=100 恒等）；
//   9.   XML <Font default> 不预缩放（KNOWN_ISSUES #22 回归门）。

#define UILIB_STATIC
#include "../DuiLib/UIlib.h"

using namespace DuiLib;

#define PUGIXML_WCHAR_MODE
#include "../3rd/pugixml/pugixml.hpp"

#include "../3rd/nanosvg/nanosvg.h"
#include "../3rd/stb/stb_image.h"
#include "../3rd/stb/stb_image_write.h"

// stb_image_write.h 全部内容位于 STB_IMAGE_WRITE_IMPLEMENTATION 之内
// （上游设计，声明不外露）；此处手工 extern "C" 声明与 Render/IRender.cpp
// 单 TU 中的实例（STBIWDEF=extern "C"）符号一致，链接经 DuiLib.lib 完成。
extern "C" unsigned char *stbi_write_png_to_mem(const unsigned char *pixels,
	int stride_bytes, int x, int y, int comp, int *out_len);

#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			++g_failed; \
			wprintf(L"[FAIL] %hs(%d): %hs\n", __FILE__, __LINE__, #cond); \
		} else { \
			++g_passed; \
		} \
	} while (0)

#define RUN_TEST(fn) \
	do { \
		int before = g_failed; \
		fn(); \
		wprintf(L"[ %s ] %hs\n", (g_failed == before) ? L" ok " : L"FAIL", #fn); \
	} while (0)

//////////////////////////////////////////////////////////////////////////
// 1/2. 注意：本仓 pugixml 的命名空间被重命名为 ui_pugi
//      （3rd/pugixml/LOCAL_CHANGES.md），测试以 ui_pugi 引用；
//      实现经 DuiLib → pugixml target 链入，链接失败即回归。
// 1. pugixml 基础解析（独立 TU 链接回归）
static void test_pugixml_parse_basic()
{
	ui_pugi::xml_document doc;
	ui_pugi::xml_parse_result result = doc.load_string(
		L"<skin><window name='main' width='800'>hello</window></skin>");
	CHECK(result);
	CHECK(wcscmp(doc.child(L"skin").child(L"window").name(), L"window") == 0);
	CHECK(wcscmp(doc.child(L"skin").child(L"window").attribute(L"name").as_string(), L"main") == 0);
	CHECK(doc.child(L"skin").child(L"window").attribute(L"width").as_int() == 800);
	CHECK(wcscmp(doc.child(L"skin").child(L"window").child_value(), L"hello") == 0);
}

// 2. pugixml 编辑 + 序列化回路（WCHAR_MODE 下 wchar_t ABI）
static void test_pugixml_edit_roundtrip()
{
	// 宽字符收集 writer（xml_writer_stream 在 save 默认编码下按字节写入
	// wostream 会错位，故显式 encoding_wchar 自行收集）
	struct wstr_writer : ui_pugi::xml_writer
	{
		std::wstring out;
		virtual void write(const void* data, size_t size)
		{
			out.append(static_cast<const wchar_t*>(data), size / sizeof(wchar_t));
		}
	};

	ui_pugi::xml_document doc;
	CHECK(doc.load_string(L"<root/>"));
	ui_pugi::xml_node node = doc.child(L"root");
	node.append_child(L"item").text().set(L"中文值");
	node.child(L"item").append_attribute(L"id").set_value(42);

	wstr_writer writer;
	doc.save(writer, L"\t", ui_pugi::format_default, ui_pugi::encoding_wchar);
	const std::wstring& out = writer.out;
	CHECK(wcsstr(out.c_str(), L"<item id=\"42\">中文值</item>") != NULL);

	CHECK(doc.load_string(out.c_str()));
	CHECK(wcscmp(doc.child(L"root").child(L"item").attribute(L"id").as_string(), L"42") == 0);
}

// 3. CDuiString 基础操作
static void test_duistring_ops()
{
	CDuiString s = L"abc";
	s += L"def";
	CHECK(s == L"abcdef");
	CHECK(s.GetLength() == 6);
	CHECK(s.Find(L"cd") == 2);
	CHECK(s.Find(L"zz") < 0);
	CHECK(s.Left(3) == L"abc");
	CHECK(s.Mid(2, 2) == L"cd");
	CHECK(s.Right(3) == L"def");
	s.Format(L"n=%d", 7);
	CHECK(s == L"n=7");
}

// 4. CDuiRect 四参构造（ATL 门控改造触点）+ 几何
static void test_duirect_ctor_and_geometry()
{
	CDuiRect rc(10, 20, 110, 70);
	CHECK(rc.GetWidth() == 100);
	CHECK(rc.GetHeight() == 50);
	CHECK(rc.left == 10 && rc.top == 20 && rc.right == 110 && rc.bottom == 70);

	CDuiRect inverted(50, 0, 10, 40);
	inverted.Normalize();
	CHECK(inverted.left == 10 && inverted.right == 50);
	CHECK(inverted.GetWidth() == 40);

	CDuiRect copied((const RECT&)rc);
	CHECK(copied.left == rc.left && copied.top == rc.top
		&& copied.right == rc.right && copied.bottom == rc.bottom);
}

// 5. 控件工厂：默认注册集合 + 未知类名
static void test_control_factory()
{
	CControlFactory* factory = CControlFactory::GetInstance();
	CHECK(factory != NULL);

	// 注册键为完整类名（CreateControl 内部 MakeLower，大小写不敏感）；
	// GetClass() 返回去 C 前缀名，两者分开核对
	const LPCTSTR keys[] = {
		L"CButtonUI", L"CLabelUI", L"CContainerUI", L"CVerticalLayoutUI",
		L"CComboUI", L"CEditUI",
	};
	const LPCTSTR classes[] = {
		L"ButtonUI", L"LabelUI", L"ContainerUI", L"VerticalLayoutUI",
		L"ComboUI", L"EditUI",
	};
	for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); ++i) {
		CControlUI* control = CControlFactory::GetInstance()->CreateControl(keys[i]);
		CHECK(control != NULL);
		if (control != NULL) {
			CHECK(wcscmp(control->GetClass(), classes[i]) == 0);
			delete control;
		}
	}
	CHECK(CControlFactory::GetInstance()->CreateControl(L"NoSuchControlUI") == NULL);

// ATL 门控下 CImageBoxExUI 不注册（默认构建注册），两态行为面锁定
#ifndef DUILIB_NO_IMAGEBOXEX
	{
		CControlUI* control = CControlFactory::GetInstance()->CreateControl(L"CImageBoxExUI");
		CHECK(control != NULL);
		delete control;
	}
#endif
}

// 6. nanosvg：基础尺寸 + gradient-stops 不变量（自 test_invariant_nanosvg.h 移植）
static void test_nanosvg_parse()
{
	char svg[] = "<svg xmlns='http://www.w3.org/2000/svg' width='100px' height='50px'>"
		"<rect width='100' height='50'/></svg>";
	NSVGimage* image = nsvgParse(svg, "px", 96.0f);
	CHECK(image != NULL);
	if (image != NULL) {
		CHECK(image->width == 100.0f);
		CHECK(image->height == 50.0f);
		nsvgDelete(image);
	}
}

static void test_nanosvg_gradient_stops_invariant()
{
	// 不变量：恶意/边界 gradient stops 输入不得崩溃（memcpy bounds check，
	// 3rd/nanosvg/LOCAL_CHANGES.md），解析要么安全成功要么返回 NULL。
	const char* payloads[] = {
		"<svg><defs><linearGradient id='g'>"
		"<stop offset='0'/><stop offset='0.1'/><stop offset='0.2'/><stop offset='0.3'/>"
		"<stop offset='0.4'/><stop offset='0.5'/><stop offset='0.6'/><stop offset='0.7'/>"
		"<stop offset='0.8'/><stop offset='0.9'/><stop offset='1.0'/><stop offset='1.1'/>"
		"<stop offset='1.2'/><stop offset='1.3'/><stop offset='1.4'/><stop offset='1.5'/>"
		"</linearGradient></defs><rect fill='url(#g)'/></svg>",
		"<svg><defs><linearGradient id='g'>"
		"<stop offset='0'/><stop offset='1'/>"
		"</linearGradient></defs><rect fill='url(#g)'/></svg>",
		"<svg><defs><linearGradient id='g'><stop offset='0.5'/></linearGradient></defs>"
		"<rect fill='url(#g)'/></svg>",
	};
	for (int i = 0; i < (int)(sizeof(payloads) / sizeof(payloads[0])); ++i) {
		char* input = _strdup(payloads[i]);
		CHECK(input != NULL);
		if (input != NULL) {
			NSVGimage* image = nsvgParse(input, "px", 96.0f);
			if (image != NULL) {
				CHECK(image->width >= 0.0f);
				nsvgDelete(image);
			}
			free(input);
		}
	}
}

// 7. stb：PNG 内存写/读回路（实现经 IRender.cpp 单 TU 链入）
static void test_stb_png_roundtrip()
{
	const int w = 4, h = 3, comp = 4;
	unsigned char pixels[4 * 3 * 4];
	for (int i = 0; i < w * h; ++i) {
		pixels[i * comp + 0] = (unsigned char)(i * 7);
		pixels[i * comp + 1] = (unsigned char)(i * 11);
		pixels[i * comp + 2] = (unsigned char)(i * 13);
		pixels[i * comp + 3] = 255;
	}
	int png_len = 0;
	unsigned char* png = stbi_write_png_to_mem(pixels, w * comp, w, h, comp, &png_len);
	CHECK(png != NULL);
	CHECK(png_len > 0);

	int w2 = 0, h2 = 0, n2 = 0;
	unsigned char* decoded = stbi_load_from_memory(png, png_len, &w2, &h2, &n2, comp);
	CHECK(decoded != NULL);
	if (decoded != NULL) {
		CHECK(w2 == w && h2 == h && n2 == comp);
		CHECK(memcmp(decoded, pixels, w * h * comp) == 0);
		stbi_image_free(decoded);
	}
	if (png != NULL)
		free(png);
}

// 8. DPI：缩放/回缩往返不变量。MulDiv 上/下两方向均为四舍五入，scale>=100
//    （DPI>=96）时往返偏差 <0.5 逻辑像素，恒等成立；scale<100（降档）不保证
//    （MulDiv(2,100,75)=3），是 KNOWN_ISSUES #23 引入 ScaleVerify 校验的依据。
static void test_dpi_scale_roundtrip()
{
	static const UINT kDpis[] = { 96, 120, 144, 168, 192, 240 };
	CDpiBase dpi;
	for (int d = 0; d < (int)(sizeof(kDpis) / sizeof(kDpis[0])); ++d) {
		dpi.SetScale(kDpis[d]);
		CHECK((int)dpi.GetScale() == MulDiv(kDpis[d], 100, 96));
		CHECK((int)dpi.GetDPI() == (int)kDpis[d]);

		int bad = 0;
		for (int x = 0; x <= 2000; ++x) {
			if (dpi.ScaleIntBack(dpi.ScaleInt(x)) != x)
				++bad;
		}
		CHECK(bad == 0);

		CDuiRect rc(13, 17, 13 + 123, 17 + 77);
		RECT r = { rc.left, rc.top, rc.right, rc.bottom };
		dpi.ScaleRect(&r);
		dpi.ScaleRectBack(&r);
		CHECK(r.left == 13 && r.top == 17 && r.right == 13 + 123 && r.bottom == 17 + 77);
	}
}

// 9. DPI：XML <Font default="true"> 的字号必须存 96 DPI 逻辑值，不得在
//    builder 侧预缩放（KNOWN_ISSUES #22）。修复前：UIDlgBuilder 先 ScaleInt
//    再 SetDefaultFont，_buildFont 构建时又 ScaleInt → 150% 下默认字体双倍
//    大小（此处断言 144 DPI 下 iSize==12，修复前为 18）。
static void test_dpi_default_font_not_prescaled()
{
	CPaintManagerWin32UI pm;
	pm.GetDPIObj()->SetScale(144);	// 150%

	CDialogBuilder builder;
	CControlUI* pRoot = builder.Create(
		_T("<VerticalLayout name='root'>"
		   "<Font id='0' name='Arial' size='12' default='true'/>"
		   "<Label name='l' text='x' width='100' height='30'/>"
		   "</VerticalLayout>"),
		NULL, NULL, &pm, NULL);
	CHECK(pRoot != NULL);
	if (pRoot != NULL)
		pm.AttachDialog(pRoot);

	UIFont* pDef = pm.GetDefaultFontInfo();
	CHECK(pDef != NULL);
	if (pDef != NULL)
		CHECK(pDef->GetSize() == 12);

	UIFont* pF0 = pm.GetFont(0);
	CHECK(pF0 != NULL);
	if (pF0 != NULL)
		CHECK(pF0->GetSize() == 12);
}

int main()
{
	wprintf(L"== duilib_test (3rd extraction regression gate) ==\n");
	RUN_TEST(test_pugixml_parse_basic);
	RUN_TEST(test_pugixml_edit_roundtrip);
	RUN_TEST(test_duistring_ops);
	RUN_TEST(test_duirect_ctor_and_geometry);
	RUN_TEST(test_control_factory);
	RUN_TEST(test_nanosvg_parse);
	RUN_TEST(test_nanosvg_gradient_stops_invariant);
	RUN_TEST(test_stb_png_roundtrip);
	RUN_TEST(test_dpi_scale_roundtrip);
	RUN_TEST(test_dpi_default_font_not_prescaled);

	wprintf(L"== %d passed, %d failed ==\n", g_passed, g_failed);
	return g_failed == 0 ? 0 : 1;
}
