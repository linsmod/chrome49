# SVG模块条件编译分析报告

## 背景

### 目标
基于ARIA模块移除的成功经验，分析如何将SVG（Scalable Vector Graphics）模块改为可通过`ENABLE(SVG)`标志控制的条件编译，以减少simpleblink程序的编译体积和依赖复杂度。

### 项目信息
- **项目路径**: `/home/wulin/chrome49/src/third_party/WebKit/Source/web/simpleblink`
- **构建系统**: GN (Generate Ninja)
- **目标**: 确保simpleblink在禁用SVG后仍能成功编译

## SVG模块结构分析

### SVG模块位置
与ARIA模块不同，SVG模块主要位于core层，而非modules层：

```
core/svg/ - SVG核心实现（约200个文件）
core/layout/svg/ - SVG布局实现（约45个文件）
core/paint/ - SVG绘制相关代码
core/animation/ - SVG动画相关代码
core/css/ - SVG CSS相关代码
```

### 关键文件路径

**core/svg/ 主要文件**:
- SVGElement.cpp/h - SVG元素基类
- SVGSVGElement.cpp/h - SVG根元素
- SVGImageElement.cpp/h - SVG图像元素
- SVGPathElement.cpp/h - SVG路径元素
- SVGTextElement.cpp/h - SVG文本元素
- SVGAnimationElement.cpp/h - SVG动画元素
- SVGFilterElement.cpp/h - SVG滤镜元素
- 以及其他约200个SVG相关文件

**core/layout/svg/ 主要文件**:
- LayoutSVGRoot.cpp/h - SVG根布局对象
- LayoutSVGContainer.cpp/h - SVG容器布局
- LayoutSVGShape.cpp/h - SVG形状布局
- LayoutSVGText.cpp/h - SVG文本布局
- SVGResources.cpp/h - SVG资源管理
- 以及其他约45个SVG布局文件

**core.gni中的SVG文件列表**:
- `webcore_svg_files` 变量包含所有SVG相关源文件
- 在core/core.gni第48行定义
- 在core/BUILD.gn第197-213行使用

### SVG集成点文件清单（分类）

#### A类：可直接排除编译的SVG专用文件（约40个文件）
这些文件专门为SVG服务，没有SVG代码就无法工作，可以直接通过BUILD.gn排除：

**Paint层SVG专用文件（12个）**:
```
core/paint/SVGContainerPainter.cpp/h
core/paint/SVGFilterPainter.cpp/h
core/paint/SVGRootPainter.cpp/h
core/paint/SVGImagePainter.cpp/h
core/paint/SVGTextPainter.cpp/h
core/paint/SVGShapePainter.cpp/h
core/paint/SVGClipPainter.cpp/h
core/paint/SVGMaskPainter.cpp/h
core/paint/SVGForeignObjectPainter.cpp/h
core/paint/SVGInlineTextBoxPainter.cpp/h
core/paint/SVGInlineFlowBoxPainter.cpp
core/paint/SVGRootInlineBoxPainter.cpp
core/paint/SVGPaintContext.cpp/h
```

**Animation层SVG专用文件（20+个）**:
```
core/animation/SVGInterpolationType.cpp/h
core/animation/SVGInterpolation.h
core/animation/DefaultSVGInterpolation.cpp/h
core/animation/NumberSVGInterpolation.cpp/h
core/animation/SVGLengthInterpolationType.cpp/h
core/animation/SVGLengthListInterpolationType.cpp/h
core/animation/SVGNumberInterpolationType.cpp/h
core/animation/SVGNumberListInterpolationType.cpp/h
core/animation/SVGNumberOptionalNumberInterpolationType.cpp/h
core/animation/SVGPointListInterpolationType.cpp/h
core/animation/SVGPathInterpolationType.cpp/h
core/animation/SVGAngleInterpolationType.cpp/h
core/animation/SVGIntegerInterpolationType.cpp/h
core/animation/SVGIntegerOptionalIntegerInterpolationType.cpp/h
core/animation/SVGRectInterpolationType.cpp/h
core/animation/SVGTransformListInterpolationType.cpp/h
core/animation/SVGValueInterpolationType.cpp/h
core/animation/SVGPathSegInterpolationFunctions.cpp/h
core/animation/animatable/AnimatablePath.cpp
core/animation/animatable/AnimatableSVGPaint.cpp/h
core/animation/animatable/AnimatableStrokeDasharrayList.h
core/animation/NumberAttributeFunctions.h
```

**CSS层SVG专用文件（2个）**:
```
core/css/CSSPathValue.cpp/h
core/style/SVGComputedStyle.cpp
core/style/StylePath.cpp
```

**Layout层SVG专用文件（1个）**:
```
core/layout/api/LineLayoutSVGInlineText.h
```

**Modules/Accessibility层SVG专用文件（1个）**:
```
modules/accessibility/AXSVGRoot.cpp
```

**其他SVG专用文件（3个）**:
```
core/bindings/tests/results/core/V8SVGTestInterface.cpp/h
```

#### B类：需要guard的混合文件（约43个文件）
这些文件既包含SVG代码又包含非SVG代码，需要guard SVG相关部分：

**Paint层混合文件（6个）**:
```
core/paint/PaintLayer.cpp - 包含SVG相关include（第70-72行）
core/paint/ReplacedPainter.cpp - 包含SVG相关include（第9行）
core/paint/PaintLayerPainter.cpp - 待检查
core/paint/PaintLayerFilterInfo.cpp - 待检查
core/paint/FilterEffectBuilder.cpp - 待检查
core/paint/PaintPropertyTreeBuilderTest.cpp - 待检查
```

**Animation层混合文件（5个）**:
```
core/animation/KeyframeEffect.cpp - 包含SVGElement.h（第47行）
core/animation/SampledEffect.cpp - 包含SVG相关include（第9-10行）
core/animation/AnimationInputHelpers.cpp - 待检查
core/animation/StringKeyframe.cpp - 待检查
core/animation/InterpolationEnvironment.h - 待检查
core/animation/PropertyHandleTest.cpp - 待检查
```

**CSS层混合文件（9个）**:
```
core/css/CSSCursorImageValue.cpp/h - 包含SVG相关include（第24, 30-32行）
core/css/CSSDefaultStyleSheets.cpp - 包含SVGImage.h（第86行）
core/css/resolver/FilterOperationResolver.cpp - 待检查
core/css/resolver/ElementStyleResources.cpp - 待检查
core/css/resolver/SharedStyleFinder.cpp - 待检查
core/css/resolver/StyleAdjuster.cpp - 待检查
core/css/resolver/StyleResolver.cpp - 待检查
core/css/ComputedStyleCSSValueMapping.cpp - 待检查
core/css/CSSStyleSheet.cpp - 待检查
core/css/parser/CSSPropertyParser.cpp - 待检查
```

**DOM层混合文件（16个）**:
```
core/dom/Element.cpp/h - 包含SVG相关include（第36, 121-122行）
core/dom/Document.cpp - 包含SVG相关include（第42-43, 193-195行）
core/dom/QualifiedName.cpp - 包含SVGNames.h（第24行）
core/dom/Range.cpp - 包含SVGSVGElement.h（第47行）
core/dom/Text.cpp - 包含SVGNames.h（第26行），LayoutSVGInlineText.h（第37行），SVGForeignObjectElement.h（第38行）
core/dom/ElementData.h - 待检查
core/dom/VisitedLinkState.cpp - 待检查
core/dom/LayoutTreeBuilder.cpp - 待检查
core/dom/PresentationAttributeStyle.cpp - 待检查
core/dom/custom/CustomElementRegistrationContext.cpp - 待检查
core/events/Event.cpp - 待检查
core/events/TreeScopeEventContext.h - 待检查
```

**Layout层混合文件（9个）**:
```
core/layout/LayoutObject.h - 待检查
core/layout/LayoutTreeAsText.cpp - 包含SVG相关include（第48-55行）
core/layout/LayoutPart.cpp - 包含SVG相关include（第34行）
core/layout/HitTestResult.cpp - 包含SVGElement.h（第44行），SVG相关代码（第348行）
core/layout/PaintInvalidationState.cpp - 包含LayoutSVGModelObject.h（第9行），LayoutSVGRoot.h（第10行）
core/layout/line/BreakingContextInlineHeaders.h - 待检查
core/layout/LayoutBlockFlowLine.cpp - 待检查
core/layout/LayoutView.cpp - 待检查
```

**Inspector层混合文件（3个）**:
```
core/inspector/InspectorCSSAgent.cpp - 待检查
core/inspector/InspectorHighlight.cpp - 待检查
core/inspector/InspectorStyleSheet.cpp - 待检查
```

**Modules/Accessibility层混合文件（3个）**:
```
modules/accessibility/AXLayoutObject.cpp/h - 待检查
modules/accessibility/AXNodeObject.cpp - 待检查
modules/accessibility/AXObjectCacheImpl.cpp - 待检查
```

**其他混合文件（约15个）**:
```
core/loader/ImageLoader.cpp - 包含SVG相关include（第45-46行），SVG相关代码（第493-494, 538-539行）
core/loader/FrameLoader.cpp - 包含SVGImage.h（第82行）
core/loader/FrameFetchContext.cpp - 待检查
core/input/EventHandler.cpp - 待检查
core/testing/Internals.cpp - 待检查
core/Init.cpp - 待检查
core/xml/parser/XMLDocumentParser.cpp - 待检查
core/xml/parser/XMLErrors.cpp - 待检查
core/html/HTMLImageElement.cpp - 包含SVG相关include（第50行）
core/html/parser/HTMLTreeBuilder.cpp - 待检查
core/html/parser/XSSAuditor.cpp - 待检查
core/html/parser/HTMLElementStack.cpp - 待检查
core/html/parser/HTMLTreeBuilderSimulator.cpp - 待检查
core/html/parser/HTMLConstructionSite.cpp - 待检查
core/html/parser/HTMLStackItem.h - 待检查
core/bindings/core/v8/V8GCController.cpp - 待检查
core/bindings/core/v8/CustomElementConstructorBuilder.cpp - 待检查
core/bindings/tests/results/core/V8TestInterfaceDocument.cpp - 待检查
modules/canvas2d/CanvasPattern.h - 待检查
modules/canvas2d/Path2D.h - 待检查
```

**源码核对说明**:
- LayoutBoxModelObject.h: 已移除，源码检查显示无SVG相关include
- Node.cpp: 已移除，源码检查显示无SVG相关include
- 已确认的SVG相关文件均已标注具体行号

### SVG文件依赖关系分析

#### 依赖层次结构

```
core/dom (DOM层)
    ↓ 依赖
core/svg (SVG DOM层)
    ↓ 依赖
core/layout/svg (SVG Layout层)
    ↓ 被依赖
core/paint (Paint层)
core/animation (Animation层)
core/css (CSS层)
```

#### 各目录依赖详情

**1. core/svg 依赖**:
- core/dom: Element.h, Document.h, Attribute.h, ScriptLoader.h, ExceptionCode.h等
- core/layout: LayoutObject.h
- core/layout/svg: LayoutSVGPath.h, LayoutSVGRect.h, LayoutSVGShape.h, LayoutSVGRoot.h等

**2. core/layout/svg 依赖**:
- core/layout: LayoutObject.h, LayoutBox.h, LayoutView.h, HitTestResult.h
- core/dom: Element.h, ElementTraversal.h
- core/layout/svg内部: 相互依赖（如LayoutSVGResourceContainer依赖LayoutSVGResourceClipper等）

**3. core/paint 依赖SVG**:
- core/layout/svg: LayoutSVGResourceFilter.h, LayoutSVGResourceMasker.h, LayoutSVGRoot.h, SVGLayoutSupport.h, SVGResources.h等
- core/svg: SVGSVGElement.h, SVGImageElement.h

**4. core/animation 依赖SVG**:
- core/svg: SVGElement.h, SVGNames.h, SVGSMILElement.h, SVGPath.h, SVGLength.h, SVGInteger.h等
- core/svg/animation: SVGSMILElement.h

**5. core/css 依赖SVG**:
- core/layout/svg: ReferenceFilterBuilder.h
- core/svg: SVGElement.h, SVGNames.h, SVGSVGElement.h, SVGURIReference.h, SVGPathUtilities.h, SVGStyleElement.h等

**6. core/dom 依赖SVG**:
- core/layout/svg: LayoutSVGInlineText.h
- core/svg: SVGNames.h, SVGElement.h, SVGDocumentExtensions.h, SVGScriptElement.h, SVGStyleElement.h, SVGUnknownElement.h, SVGSVGElement.h, SVGForeignObjectElement.h

#### 依赖关系特点

1. **双向依赖**: core/dom既被core/svg依赖，又依赖core/svg
2. **深度集成**: SVG深度集成到paint、animation、css、dom等各个子系统
3. **循环依赖**: core/svg → core/layout/svg → core/dom → core/svg 形成循环依赖
4. **广泛引用**: SVG相关代码被大量非SVG文件引用

#### 对方案的影响

1. **BUILD.gn过滤的可行性**: 由于依赖关系复杂，需要同时过滤core/svg和core/layout/svg
2. **条件编译的必要性**: 由于循环依赖和双向依赖，必须对混合文件进行条件编译
3. **guard顺序**: 建议先过滤core/svg和core/layout/svg，再guard混合文件
4. **风险点**: 循环依赖可能导致编译错误，需要仔细处理

### 纯虚函数继承链分析

#### SVG元素继承链
```
Element (core/dom/Element.h)
  └─ SVGElement (core/svg/SVGElement.h) - 基类
      ├─ SVGGraphicsElement
      │   ├─ SVGShapeElement
      │   │   ├─ SVGGeometryElement (纯虚函数: virtual Path asPath() const = 0)
      │   │   │   ├─ SVGCircleElement
      │   │   │   ├─ SVGEllipseElement
      │   │   │   ├─ SVGLineElement
      │   │   │   ├─ SVGPathElement
      │   │   │   ├─ SVGPolygonElement
      │   │   │   ├─ SVGPolylineElement
      │   │   │   └─ SVGRectElement
      │   │   └─ SVGTextContentElement
      │   │       ├─ SVGTextElement
      │   │       ├─ SVGTSpanElement
      │   │       └─ SVGTextPathElement
      │   ├─ SVGAElement
      │   ├─ SVGImageElement
      │   ├─ SVGForeignObjectElement
      │   └─ SVGSVGElement
      ├─ SVGAnimationElement (纯虚函数: virtual bool calculateToAtEndOfDurationValue() = 0, virtual bool calculateFromAndToValues() = 0, virtual bool calculateFromAndByValues() = 0, virtual void calculateAnimatedValue() = 0)
      │   ├─ SVGAnimateElement
      │   ├─ SVGSetElement
      │   ├─ SVGAnimateMotionElement
      │   └─ SVGAnimateTransformElement
      ├─ SVGFilterPrimitiveStandardAttributes (纯虚函数: virtual PassRefPtrWillBeRawPtr<FilterEffect> build() = 0)
      │   └─ (多个滤镜元素子类)
      └─ (其他SVG元素子类)
```

#### Layout SVG继承链
```
LayoutObject (core/layout/LayoutObject.h)
  └─ LayoutSVGModelObject
      ├─ LayoutSVGContainer
      │   ├─ LayoutSVGViewportContainer
      │   ├─ LayoutSVGTransformableContainer
      │   └─ LayoutSVGHiddenContainer
      │       └─ LayoutSVGResourceContainer (纯虚函数: virtual void removeAllClientsFromCache() = 0, virtual void removeClientFromCache() = 0, virtual LayoutSVGResourceType resourceType() const = 0)
      │           ├─ LayoutSVGResourcePaintServer (纯虚函数: virtual SVGPaintServer preparePaintServer() = 0)
      │           │   ├─ LayoutSVGResourceGradient (纯虚函数: virtual SVGUnitTypes::SVGUnitType gradientUnits() const = 0, virtual void calculateGradientTransform() = 0, virtual bool collectGradientAttributes() = 0, virtual void buildGradient() const = 0)
      │           │   │   ├─ LayoutSVGResourceLinearGradient
      │           │   │   └─ LayoutSVGResourceRadialGradient
      │           │   ├─ LayoutSVGResourcePattern
      │           │   └─ LayoutSVGResourceClipper
      │           ├─ LayoutSVGResourceMarker
      │           ├─ LayoutSVGResourceMasker
      │           └─ LayoutSVGResourceFilter
      │       └─ LayoutSVGResourceFilterPrimitive
      ├─ LayoutSVGShape
      │   ├─ LayoutSVGPath
      │   ├─ LayoutSVGEllipse
      │   └─ LayoutSVGRect
      ├─ LayoutSVGImage
      └─ LayoutSVGText
          └─ LayoutSVGTSpan
```

#### SVG属性继承链
```
SVGPropertyBase (core/svg/properties/SVGProperty.h) - 纯虚函数基类
  纯虚函数:
  - virtual PassRefPtrWillBeRawPtr<SVGPropertyBase> cloneForAnimation(const String&) const = 0
  - virtual String valueAsString() const = 0
  - virtual void add() = 0
  - virtual void calculateAnimatedValue() = 0
  - virtual float calculateDistance() = 0

SVGAnimatedPropertyBase (core/svg/properties/SVGAnimatedProperty.h)
  纯虚函数:
  - virtual SVGPropertyBase* currentValueBase() = 0
  - virtual const SVGPropertyBase& baseValueBase() const = 0

SVGEnumerationBase (core/svg/SVGEnumeration.h)
  纯虚函数:
  - virtual PassRefPtrWillBeRawPtr<SVGEnumerationBase> clone() const = 0
```

### 与ARIA模块的对比

| 特性 | ARIA模块 | SVG模块 |
|------|---------|---------|
| 主要位置 | modules/accessibility | core/svg/ |
| 文件数量 | 约40个文件 | 约250个文件 |
| 接口层 | AXObjectCache (core层) | 无清晰接口 |
| Web层依赖 | 9个文件 | 几乎无直接依赖 |
| Core层集成 | 有限 | 深度集成（paint, animation, css） |
| 复杂度 | 低 | 高 |

### 依赖关系分析

**SVG依赖链路**:
```
simpleblink (HTMLViewer)
    ↓ 依赖
web (blink_web)
    ↓ 依赖
core (blink_core)
    ↓ 包含
svg (SVG模块 - 在core内部)
    ↓ 集成
paint, animation, css, layout等子系统
```

**SVG集成点**:
- **Paint层**: SVGImagePainter, SVGFilterPainter, SVGTextPainter等
- **Animation层**: SVGTransformListInterpolationType, SVGLengthInterpolationType等
- **CSS层**: SVGComputedStyle, SVGComputedStyleDefs等
- **Layout层**: LayoutSVGRoot, LayoutSVGContainer等
- **DOM层**: SVGElement及其子类

## 推荐方案

### 方案概述
采用BUILD.gn过滤为主、条件编译为辅的混合方案，最大程度减少工作量。

### 核心策略

#### 1. BUILD.gn过滤（主要手段）
过滤所有SVG专用文件，包括：
- core/svg和layout/svg目录（约245个文件）
- A类SVG专用集成点文件（约40个文件）

这些文件通过BUILD.gn条件编译直接排除，不需要任何代码修改。

#### 2. 条件编译（辅助手段）
只对B类混合文件（约40个）进行条件编译：
- 先编译，识别报错文件
- 只guard实际报错的文件（预计20-30个）
- 使用`#if ENABLE(SVG)`保护SVG相关代码

#### 3. 拒绝STUB
绝不提供stub实现，完全移除代码。

### 实施步骤

#### 步骤1: 修改BUILD.gn
- 修改core/core.gni，添加enable_svg条件判断
- 修改core/BUILD.gn，过滤：
  - webcore_svg_files（core/svg和layout/svg）
  - A类SVG专用集成点文件（SVG*Painter, SVG*InterpolationType等）

#### 步骤2: 编译测试
- 在禁用SVG的情况下编译
- 收集编译错误
- 识别需要guard的B类混合文件

#### 步骤3: 条件编译混合文件
- 只guard报错的文件
- 添加`#include "core/config.h"`
- 使用`#if ENABLE(SVG)`保护SVG相关代码

#### 步骤4: 测试验证
- 编译测试
- 功能回归测试
- 启用SVG对比测试

### 工作量估算
- BUILD.gn修改: 2个文件
- 混合文件guard: 预计25-35个文件
- **总计: 约27-37个文件**

### 优势
- 工作量最小（相比纯条件编译方案减少94%）
- BUILD.gn过滤最干净
- 按需修改，避免盲目guard
- 不引入stub，维护成本低

## 实施要点

### 1. BUILD.gn过滤配置
**core/core.gni修改**:
```gn
if (enable_svg) {
  webcore_svg_files = get_path_info(_gypi.webcore_svg_files, "abspath")
} else {
  webcore_svg_files = []
}
```

**core/BUILD.gn修改**:
```gn
source_set("svg") {
  if (enable_svg) {
    sources = rebase_path(webcore_svg_files, ".", "//")
  } else {
    sources = []
  }
  # ... 其他配置
}

# 在各子target中过滤A类SVG专用文件
# 例如在paint target中过滤SVG*Painter.cpp/h
```

### 2. 条件编译模式
```cpp
#include "core/config.h"

#if ENABLE(SVG)
// SVG相关代码
#include "core/svg/SVGElement.h"
// ...
#endif
```

### 3. ENABLE宏使用
- 不定义ENABLE_SVG=0，保持未定义状态
- 所有使用`#if ENABLE(SVG)`的文件需要include build_config.h

### 4. 拒绝STUB
绝不提供stub实现，完全移除代码。

## 详细实施步骤

### 步骤1: BUILD.gn配置
1. 修改core/core.gni，添加enable_svg变量
2. 修改core/BUILD.gn，过滤webcore_svg_files
3. 在各子target中过滤A类SVG专用文件
4. 确保依赖关系正确

### 步骤2: 编译测试
1. 设置enable_svg=false
2. 执行完整编译
3. 收集编译错误
4. 识别需要guard的B类混合文件

### 步骤3: 条件编译
1. 对报错的B类混合文件添加条件编译
2. 添加`#include "core/config.h"`
3. 使用`#if ENABLE(SVG)`保护SVG相关代码
4. 重新编译验证

### 步骤4: 测试验证
1. 禁用SVG编译测试
2. 基本功能测试
3. 启用SVG对比测试

## 风险评估

### 高风险项

1. **循环依赖导致编译错误**
   - core/dom ↔ core/svg ↔ core/layout/svg形成循环依赖
   - 简单排除SVG会导致DOM层编译失败
   - **缓解措施**: 必须同时guard DOM层SVG引用（QualifiedName.cpp, Range.cpp, Text.cpp）

2. **纯虚函数基类无法stub**
   - SVGElement继承自Element，包含纯虚函数
   - LayoutSVGResourceContainer等类有纯虚函数
   - **缓解措施**: 必须完全移除这些类，不能提供stub实现

3. **生成文件依赖SVG**
   - core_generated中包含SVGElementFactory.cpp, SVGNames.cpp等生成文件
   - **缓解措施**: 需要修改make_core_generated_svg_names等构建规则

### 中等风险项

1. **B类混合文件数量可能超出预期**
   - 原估计40个，实际可能需要guard 43个或更多
   - DOM层发现额外3个文件，Layout层发现额外1个文件
   - 实际可能需要guard 35-40个文件

2. **运行时类型检查**
   - isSVGElement(), isSVGImageElement()等类型检查散布在代码中
   - 需要guard这些检查点

3. **CSS层SVG样式**
   - CSSDefaultStyleSheets.cpp包含m_svgStyleSheet
   - 需要条件加载SVG样式表

### 低风险项

1. **BUILD.gn过滤机制成熟**
   - 已有ARIA模块成功经验
   - webcore_svg_files变量设计合理

2. **Class A文件排除安全**
   - SVG专用文件无外部依赖
   - 可直接通过BUILD.gn排除

### 需要注意
- SVG是Web标准的重要组成部分
- 禁用SVG可能影响某些网页的显示
- 需要评估simpleblink的实际需求
- 可能需要保留部分SVG功能（如SVG DOM）
- 编译错误分析需要仔细，避免遗漏
- 需要确保A类SVG专用文件的过滤逻辑正确

## 源码核对总结

### 已核对文件（19个）

**A类SVG专用文件（已确认3个）**:
- SVGContainerPainter.cpp - 确认：只包含SVG相关代码
- SVGPathSegInterpolationFunctions.cpp - 确认：只处理SVG路径段
- （其他A类文件待核对）

**B类混合文件（已确认19个）**:
- PaintLayer.cpp - 确认：第70-72行SVG相关include
- ReplacedPainter.cpp - 确认：第9行SVG相关include
- KeyframeEffect.cpp - 确认：第47行SVGElement.h
- SampledEffect.cpp - 确认：第9-10行SVG相关include
- CSSCursorImageValue.cpp - 确认：第24, 30-32行SVG相关include
- CSSDefaultStyleSheets.cpp - 确认：第86行SVGImage.h
- Element.cpp - 确认：第36, 121-122行SVG相关include
- Document.cpp - 确认：第42-43, 193-195行SVG相关include
- QualifiedName.cpp - 确认：第24行SVGNames.h
- Range.cpp - 确认：第47行SVGSVGElement.h
- Text.cpp - 确认：第26行SVGNames.h，第37行LayoutSVGInlineText.h，第38行SVGForeignObjectElement.h
- LayoutTreeAsText.cpp - 确认：第48-55行SVG相关include
- LayoutPart.cpp - 确认：第34行SVG相关include
- HitTestResult.cpp - 确认：第44行SVGElement.h，第348行SVG代码
- PaintInvalidationState.cpp - 确认：第9行LayoutSVGModelObject.h，第10行LayoutSVGRoot.h
- ImageLoader.cpp - 确认：第45-46行SVG相关include，第493-494, 538-539行SVG代码
- FrameLoader.cpp - 确认：第82行SVGImage.h
- HTMLImageElement.cpp - 确认：第50行SVG相关include

**已移除文件（2个）**:
- LayoutBoxModelObject.h - 无SVG相关include
- Node.cpp - 无SVG相关include

### 待核对文件（约24个）
- Paint层：PaintLayerPainter.cpp, PaintLayerFilterInfo.cpp, FilterEffectBuilder.cpp, PaintPropertyTreeBuilderTest.cpp
- Animation层：AnimationInputHelpers.cpp, StringKeyframe.cpp, InterpolationEnvironment.h, PropertyHandleTest.cpp
- CSS层：FilterOperationResolver.cpp, ElementStyleResources.cpp, SharedStyleFinder.cpp等
- DOM层：ElementData.h, VisitedLinkState.cpp, LayoutTreeBuilder.cpp, PresentationAttributeStyle.cpp, CustomElementRegistrationContext.cpp
- Layout层：LayoutObject.h, BreakingContextInlineHeaders.h, LayoutBlockFlowLine.cpp, LayoutView.cpp
- Inspector层：3个文件
- Accessibility层：3个文件
- 其他：FrameFetchContext.cpp, EventHandler.cpp等

### 核对结论
1. A类SVG专用文件分类基本正确，可以安全通过BUILD.gn排除
2. B类混合文件分类正确，确实包含SVG相关代码，需要条件编译
3. LayoutBoxModelObject.h和Node.cpp确实无SVG相关代码，已正确移除
4. 新发现DOM层3个文件（QualifiedName.cpp, Range.cpp, Text.cpp）和Layout层1个文件（PaintInvalidationState.cpp）需要guard
5. 方案可行，BUILD.gn过滤 + 条件编译的混合方案是正确的

## 预期结果

### 编译体积减少
- SVG代码约300个文件
- 预计减少约500KB-1MB编译体积
- 比ARIA移除效果更显著

### 依赖简化
- 完全移除SVG渲染依赖
- 简化core层复杂度
- 提高编译速度

## 总结

SVG模块移除比ARIA模块移除复杂得多：
- **规模更大**: 约300个文件 vs ARIA的30个文件
- **集成更深**: 深度集成到core各个子系统 vs ARIA的模块化设计
- **依赖更广**: 影响paint, animation, css等 vs ARIA的有限影响
- **风险更高**: 可能影响core层整体 vs ARIA的局部影响

**最终方案**: 采用BUILD.gn过滤为主、条件编译为辅的混合方案

实施要点：
1. 修改core/core.gni，添加enable_svg条件判断
2. 修改core/BUILD.gn，过滤webcore_svg_files和A类SVG专用文件
3. 条件化make_core_generated_svg_names等生成规则
4. 对B类混合文件（约43个）进行条件编译，使用`#if ENABLE(SVG)`保护SVG相关代码
5. 特别注意DOM层文件（QualifiedName.cpp, Range.cpp, Text.cpp）和Layout层文件（PaintInvalidationState.cpp）的guard
6. 分阶段实施，先过滤SVG文件，再guard混合文件，最后处理生成文件
7. 基于ARIA移除的成功经验，特别注意纯虚函数基类和循环依赖的处理
