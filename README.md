# DuiLib_DuiEditor (fork)

[![build](https://github.com/luiox/DuiLib_DuiEditor/actions/workflows/build.yml/badge.svg)](https://github.com/luiox/DuiLib_DuiEditor/actions/workflows/build.yml)

> 本仓库是 [xfcanyue/DuiLib_DuiEditor](https://github.com/xfcanyue/DuiLib_DuiEditor) 的下游 fork，
> 在上游基础上维护：Windows 构建脚本与 CI、实测 bug 修复、已知问题清单。
> 上游合并策略：保持最小 diff，逐条对照 `docs/KNOWN_ISSUES.md`。

## 构建（Windows / MSVC / GDI 后端）

```powershell
xmake f -p windows -a x64 -m release   # 首次配置；debug 同理
xmake                                  # 构建静态库 DuiLib.lib
xmake f --kind=shared                  # 如需动态库
```

* 产物：静态库（使用方需定义 `UILIB_STATIC`）。
* Win32 构建排除 SDL 后端源码（`*Sdl*.cpp`，由 `DUILIB_SDL` 宏门控）；
  Linux/macOS 的 SDL 构建走 `DuiLib/CMakeLists.txt`。
* CI：GitHub Actions（windows-latest，release/debug 双矩阵），见
  `.github/workflows/build.yml`。
* 已知问题与修复状态：`docs/KNOWN_ISSUES.md`。

---

# DuiEditor（上游原文）

### 一. 开源说明


```
本项目是针对DuiLib开发的可视化界面设计工具，可实际工作中使用。
同时本项目维护了一个DuiLib库。
欢迎加入duilib设计器交流群：819272442，一起探讨和开发。
```

### 二.  项目帮助	

```
相关教程与说明：	
duilib设计器 DuiEditor简易教程 (DuiDesigner) （一）
https://blog.csdn.net/mmcanyu/article/details/106758857

duilib设计器 DuiEditor简易教程 (DuiDesigner) （二）
https://blog.csdn.net/mmcanyu/article/details/106799975

duilib设计器 DuiEditor简易教程 (DuiDesigner) （三）
https://blog.csdn.net/mmcanyu/article/details/106843748

DuiEditor高级教程
https://blog.csdn.net/mmcanyu/article/details/106976013

duilib仿百度网盘界面
https://blog.csdn.net/mmcanyu/article/details/108905986

DuiLib中图片属性的编辑
https://blog.csdn.net/mmcanyu/article/details/111148886

正式发布DuiLib脚本系统 结合angelscript 
https://blog.csdn.net/mmcanyu/article/details/112685524

DuiLib跨平台教程
https://blog.csdn.net/mmcanyu/article/details/162272826

```

### 三. 项目编译说明：
```
DuiEditor2010.sln	VS2010解决方案  
DuiEditor2019.sln	VS2019解决方案  

编译选项说明：  
Debug				多字节 - 调试  
Release				多字节 - 发行  
UnicodeDebug		Unicode - 调试  
UnicodeRelease		Unicode - 发行  
xsDebug				静态库 - 多字节 - 调试  
xsRelease			静态库 - 多字节 - 发行  
xsUnicodeDebug		静态库 - Unicode - 调试  
xsUnicodeRelease	静态库 - Unicode - 发行  

除非为了兼容老项目，新项目优先使用Unicode版本。 

重要提醒：设计器非Unicode版本会出错！！！ Duilib库和相关插件无此限制。

```
### 四. 跨平台

Linux and macos
```
1, 编译安装SDL3
​cd 3rd
bash makeSDL.sh 或者 sh makeSDL.sh
SDL3会安装到 /usr/local/include 和 /usr/local/lib  

2，编译DuiLib
cd DuiLib

sh makeDuiLib.sh
sudo cmake --install build

或者使用xmake编译
xmake
xmake install --root

MacOS可以使用xcode编译，项目路径：MacOS/DuiLib/DuiLib.xcodeproj

3，跨平台范例
Linux/gcc
MacOS/Test
MacOS/TestApp

4, 教程
https://blog.csdn.net/mmcanyu/article/details/162272826

```

Android
```
Windows平台交叉编译

1, 编译SDL3, SDL3_ttf
makeSDL_android.bat
makeSDL_ttf_android.bat

2, 编译DuiLib
makeDuiLib_android.bat

3, Android范例
Android\duidemo

4, 教程
https://blog.csdn.net/mmcanyu/article/details/162452831

```
### 五. 重要说明
```
设计器创建控件和修改属性依据`bin\DuiLib.xml`，理论上只要遵循这个约定的duilib版本都可以使用这款设计器。
```