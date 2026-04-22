# ARIA模块移除分析报告

## 背景

### 目标
从`simpleblink`程序中移除ARIA（Accessible Rich Internet Applications）模块依赖，减少编译体积和依赖复杂度。

### 项目信息
- **项目路径**: `/home/wulin/chrome49/src/third_party/WebKit/Source/web/simpleblink`
- **构建系统**: GN (Generate Ninja)
- **目标**: 确保`simpleblink`在移除ARIA依赖后仍能成功编译

## 依赖关系分析

### 依赖链路
```
simpleblink (HTMLViewer)
    ↓ 依赖
web (blink_web)
    ↓ 依赖
modules (blink_modules)
    ↓ 包含
accessibility (ARIA模块)
```

### 架构关系

```
core/dom/AXObjectCache.h (抽象基类 - core层)
    ↓ 工厂模式注册
modules/accessibility/AXObjectCacheImpl.h (具体实现 - modules层)
    ↓ 初始化调用
modules/InitModules.cpp::init()
    ↓ 调用
core层各文件 (existingAXObjectCache())
    ↓ 调用
web层各文件 (accessibility APIs)
```

### 关键文件路径

**modules层accessibility代码**:
- `/home/wulin/chrome49/src/third_party/WebKit/Source/modules/accessibility/` (约40个文件)
- 主要文件: AXObject.cpp/h, AXObjectCacheImpl.cpp/h, AXLayoutObject.cpp/h等

**modules层初始化**:
- `/home/wulin/chrome49/src/third_party/WebKit/Source/modules/InitModules.cpp`
  - 第15行: `#include "modules/accessibility/AXObjectCacheImpl.h"`
  - 第39行: `AXObjectCache::init(AXObjectCacheImpl::create);`

**core层accessibility接口**:
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/AXObjectCache.h` (抽象基类)
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/AXObjectCache.cpp` (工厂实现)

**web层accessibility依赖** (9个文件):
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/ChromeClientImpl.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebViewImpl.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebPagePopupImpl.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebAXObject.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebDevToolsAgentImpl.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebDocument.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebNode.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/TextFinder.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/AssertMatchingEnums.cpp`

**core层accessibility调用** (约12个文件):
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/Document.h/cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/page/ChromeClient.h`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/Element.h/cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/Node.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/layout/LayoutObject.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/layout/LayoutBlockFlowLine.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/layout/LayoutListBox.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/html/HTMLTextFormControlElement.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/html/HTMLSelectElement.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/paint/PaintLayerScrollableArea.cpp`

## 方案分析

### 方案A: 创建不包含accessibility的web子集库
**思路**: 在web/BUILD.gn创建web_minimal目标，排除accessibility依赖文件

**问题**:
- 刀太大，WebViewImpl、ChromeClientImpl等核心文件被排除会导致无法编译
- 需要提供大量stub实现，工作量巨大

**结论**: 不可行

### 方案B: 创建不包含accessibility的modules子集
**思路**: 在modules/BUILD.gn创建modules_core目标，排除accessibility文件

**问题**:
- V8绑定生成器生成合并文件（V8GeneratedModulesBindings01-19.cpp）
- IDL依赖会传递到生成的绑定代码
- 过滤accessibility文件可能导致绑定代码不完整
- 需要复制或修改IDL生成器逻辑，复杂度极高

**结论**: 不可行

### 方案C: 仅使用编译标志 (ENABLE_ACCESSIBILITY=0)
**思路**: 在simpleblink BUILD.gn添加编译标志，依赖链接器gc-sections移除未使用代码

**问题**:
- web层包含accessibility头文件，即使代码被条件编译排除，头文件中的符号声明仍可能被保留
- modules的accessibility代码可能因为头文件依赖而被保留
- 无法保证编译体积自动变小

**结论**: 不充分

### 方案D: 修改IDL绑定生成器注入条件编译
**思路**: 研究IDL绑定生成器，注入ENABLE_ACCESSIBILITY条件编译

**研究发现**:
- 生成器支持RuntimeEnabled扩展属性
- 模板会根据runtime_enabled_function生成条件编译代码
- 但accessibility主要是C++代码，不是IDL生成的
- 只有一个IDL文件: `accessibility/testing/InternalsAccessibility.idl`（测试用）
- 生成器无法处理C++源文件的条件编译

**结论**: 不可行

### 方案E: 手动条件编译 + 编译标志 (推荐)
**思路**: 在web层和core层的accessibility相关代码中添加`#if ENABLE(ACCESSIBILITY)`保护

**修改范围**:
- web层: 9个文件
- core层: 约12个文件
- modules层: 1个文件 (InitModules.cpp)
- 总计: 约22个文件

**优点**:
- 明确移除accessibility代码
- 编译时完全排除，不依赖链接器优化
- 可逆，容易回退

**缺点**:
- 工作量较大
- 需要仔细测试确保不破坏现有功能

**结论**: 可行但工作量大

### 方案F: 只修改web层 + 提供core层stub (折中方案)
**思路**:
1. web层9个文件添加`#if ENABLE(ACCESSIBILITY)`
2. core层AXObjectCache保持不变，但提供空实现stub
3. simpleblink BUILD.gn添加`defines = [ "ENABLE_ACCESSIBILITY=0" ]`
4. modules/InitModules.cpp条件编译init调用

**优点**:
- 避免修改core层的10+个文件
- 通过stub保证编译通过
- 链接时gc-sections会移除未使用的accessibility代码
- 工作量适中

**缺点**:
- core层仍包含accessibility接口定义
- 编译体积减少不如完全移除彻底

**结论**: 不采用（用户要求不用stub）

## 最终方案

### 采用方案: 方案E (手动条件编译 + 编译标志)

### 实施步骤

#### 1. 修改simpleblink BUILD.gn
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/web/simpleblink/BUILD.gn`

在configs后添加:
```gn
defines = [ "ENABLE_ACCESSIBILITY=0" ]
```

#### 2. 修改modules/InitModules.cpp
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/modules/InitModules.cpp`

条件编译accessibility相关代码:
```cpp
#if ENABLE(ACCESSIBILITY)
#include "modules/accessibility/AXObjectCacheImpl.h"
#endif

void ModulesInitializer::init()
{
    // ... 其他初始化代码 ...

#if ENABLE(ACCESSIBILITY)
    AXObjectCache::init(AXObjectCacheImpl::create);
#endif

    // ... 其他初始化代码 ...
}
```

#### 3. 修改web层9个文件

**3.1 ChromeClientImpl.cpp**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/web/ChromeClientImpl.cpp`

```cpp
#if ENABLE(ACCESSIBILITY)
#include "modules/accessibility/AXObject.h"
#endif

void ChromeClientImpl::postAccessibilityNotification(AXObject* obj, AXObjectCache::AXNotification notification)
{
#if ENABLE(ACCESSIBILITY)
    // 原有实现
#endif
}
```

**3.2 WebViewImpl.cpp**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebViewImpl.cpp`

```cpp
#if ENABLE(ACCESSIBILITY)
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#endif

WebAXObject WebViewImpl::accessibilityObject()
{
#if ENABLE(ACCESSIBILITY)
    // 原有实现
#else
    return WebAXObject();
#endif
}
```

**3.3 WebPagePopupImpl.cpp**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebPagePopupImpl.cpp`

```cpp
#if ENABLE(ACCESSIBILITY)
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#endif

// 条件编译accessibility相关方法
```

**3.4 WebAXObject.cpp**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebAXObject.cpp`

整个文件用`#if ENABLE(ACCESSIBILITY)`包裹

**3.5 WebDevToolsAgentImpl.cpp**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebDevToolsAgentImpl.cpp`

```cpp
#if ENABLE(ACCESSIBILITY)
#include "modules/accessibility/InspectorAccessibilityAgent.h"
#endif
```

**3.6 WebDocument.cpp, WebNode.cpp, TextFinder.cpp, AssertMatchingEnums.cpp**
**文件**:
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebDocument.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/WebNode.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/TextFinder.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/web/AssertMatchingEnums.cpp`

移除或条件编译accessibility头文件包含和相关代码

#### 4. 修改core层12个文件

**4.1 core/dom/AXObjectCache.h**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/AXObjectCache.h`

整个类定义用`#if ENABLE(ACCESSIBILITY)`包裹

**4.2 core/dom/AXObjectCache.cpp**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/AXObjectCache.cpp`

AXObjectCache.cpp文件用`#if ENABLE(ACCESSIBILITY)`包裹, 
为让让其他文件仍然可以直接include AXObjectCache.h它从而减少整体代码更改。AXObjectCache.h特殊处理过了.

**4.3 core/dom/Document.h/cpp**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/Document.h`
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/Document.cpp`

条件编译existingAXObjectCache()声明和实现

**4.4 core/page/ChromeClient.h**
**文件**: `/home/wulin/chrome49/src/third_party/WebKit/Source/core/page/ChromeClient.h`

条件编译postAccessibilityNotification虚函数声明

**4.5 其他core层文件**
**文件**:
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/Element.h/cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/dom/Node.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/layout/LayoutObject.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/layout/LayoutBlockFlowLine.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/layout/LayoutListBox.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/html/HTMLTextFormControlElement.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/html/HTMLSelectElement.cpp`
- `/home/wulin/chrome49/src/third_party/WebKit/Source/core/paint/PaintLayerScrollableArea.cpp`

条件编译existingAXObjectCache()调用

#### 5. 编译测试
```bash
py2 #必须执行以切换到python2以让chrome构建系统正确运行
ninja -C out/Release simpleblink
```

#### 6. 验证
- 确保编译通过
- 检查生成的HTMLViewer可执行文件
- 验证basic HTML渲染功能正常

## 预期结果

### 编译体积减少
- web层accessibility代码完全排除
- core层accessibility代码完全排除
- modules层accessibility代码通过gcc自动的链接器优化移除
- 预计减少约200-300KB编译体积

### 依赖简化
- 完全移除ARIA模块的编译依赖
- 简化构建配置
- 提高编译速度

## 风险评估

### 中等风险
- simpleblink本身不使用accessibility功能
- 修改仅限于条件编译，不改变核心逻辑
- 可通过ENABLE_ACCESSIBILITY标志轻松回退

### 需要注意
- 需要修改约22个文件（web层9个 + core层12个 + modules层1个）
- 需要确保所有accessibility调用都被条件编译覆盖
- 需要仔细测试确保不破坏core层的其他功能
- core层的AXObjectCache接口被完全移除，可能影响其他依赖core的组件

## 总结

经过深入分析，确定采用**方案E（手动条件编译 + 编译标志）**来移除ARIA模块依赖。该方案通过条件编译web层和core层的accessibility代码，结合gcc自动的链接器优化移除未使用的modules代码，在保证编译通过的同时有效减少依赖和编译体积。修改范围较大（约22个文件），但能彻底移除accessibility依赖，风险可控，是当前最彻底的可行方案。

不论哪个基类有相关纯虚函数，guard it。 No stub in our goals.
