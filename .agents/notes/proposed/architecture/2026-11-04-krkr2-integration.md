# krkr2 接入任务分解

Status: proposed

## 问题

把 Ciallang 作为可切换 TJS2 后端接入 [2468785842/krkr2](https://github.com/2468785842/krkr2)。
接入位置与验收门槛见[接入说明](../../../../doc/krkr2-integration.md)，本 note 跟踪各阶段剩余工作。

## 已知事实与证据

- 接入说明定义了阶段划分与完成标准：目标宿主在相同资源与脚本下通过验收，
  且能切回原 TJS2 差分；单元测试、fixture 匹配与 SDK 链接各自证明各自范围，
  不单独构成"核心可替换"。
- 各阶段均未开始实现。

## 建议下一步

按接入说明阶段顺序：

1. 双向 `iTJSDispatch2` / VariantClosure 桥：对象身份、AddRef/Release 与 GC 根、flags、
   Operation、枚举、NativeInstanceSupport、ClassInstanceInfo、无效化和异常转换。
2. 字节码 proxy 的 direct/indirect 回落、缺失成员 typeof、一次 Operation 派发与
   动态类体/父类表达式。
3. 对接 TVP 文本/二进制读写流；接入 XP3、模式串、字节码探测与源码身份映射。
   Scripts 文本读取器已有入口；Array/Dictionary 保存与加载仍直接使用文件系统。
4. 目标仓库增加可切换后端并验证 startup/Initialize、KAG、Window/Layer/Timer 回调、
   插件、存档和退出回收；桌面通过后验证 Android ARM64/x86_64。
5. 补齐其余 Unicode 边界（sprintf 的字符/宽度/精度、大小写及文本存取），
   建立目标 TJS2 与真实游戏语料的持续差分验收。

## 完成条件

接入说明的完成标准：宿主验收通过且可切回原 TJS2 差分；
全部阶段完成后本 note 移入 `implemented/` 并同步接入说明的实施状态。

## 风险或备选方案

GC 与引用计数边界、一次 Operation 派发和异常转换是最易出兼容缺陷的位置；
阶段 4 之前不修改目标仓库的默认构建行为。
