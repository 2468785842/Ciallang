## 总体目标
在现有“词法 → Pratt解析 → AST → 字节码 → VM”的成熟架构上，扩展对象系统与调用/访问语义，使其满足老规范：统一 Object 基类与三类派生（Class/Function/Property），支持类定义、成员表、继承、动态 this 绑定、属性 get/set 与访问控制；保持无闭包设计与动态作用域链；完善智能指针与引用计数并规避循环引用；补充测试、性能与内存检测。

## 设计原则
- 复用：尽量复用 `TjsValue/Object/Function`、`IRGenerator` 与 `VMState` 的能力，避免破坏现有前后端分层。
- 可插拔：对象模型与属性访问通过受控的字节码与运行时接口接入，便于增量迁移与回滚。
- 无闭包：函数不捕获外部环境，作用域解析停留在生成期与调用期的符号/寄存器窗口。
- 动态 this：调用点决定 `this`，以调用字节码约定与帧初始化实现。

## 模块改造
### 对象系统
- 新增 `src/types/RuntimeObject.hpp/.cpp`
  - `class RuntimeObject : public Object`，统一承载三类：`ClassObject`、`FunctionObject`、`PropertyObject`（可用 `enum RuntimeObjectKind` 标识）。
  - 通用字段：`name`、`metadata`（`unordered_map<string, TjsValue>`）、`scopeChain`（`vector<weak_ptr<ScopeNode>>`）。
- 新增 `ClassObject`
  - 成员属性表：`unordered_map<string, shared_ptr<PropertyObject>>`。
  - 成员函数表：`unordered_map<string, shared_ptr<FunctionObject>>`。
  - 继承：`weak_ptr<ClassObject> base` 与线性化查找策略（先本类，后基类）。
- 扩展 `FunctionObject`
  - 字段：`params`（标识符列表）、`bodyChunk`（沿用 `Function` 的 `Chunk`）、`dynamicThis`（调用期注入）。
  - 无闭包：不持有外部局部，只依赖符号表与调用帧寄存器窗口。
- 新增 `PropertyObject`
  - 字段：`value`（基本值）、可选 `getter`/`setter`（`FunctionObject`）、访问标志位（`enum AccessFlag { Public, Protected, Private }`）。

### 作用域与语义
- 引入 `ScopeNode`（轻量结构）记录链与符号视图，挂在运行时对象 `scopeChain`，供语义分析与运行时查找。
- 解析期保持现有 `Parser` 行为；在生成期的 `IRGenerator` 增加：
  - 类声明生成：构造 `ClassObject`，注册到全局符号表；为成员声明发射属性/函数注册字节码。
  - `this` 注入：在 `Call` 前约定将接收者寄存器写入帧的 `thisReg`，在函数入口将其绑定到 `FunctionObject` 的运行期上下文。
  - 成员访问：将 `obj.prop` 解析为两阶段字节码：先取对象寄存器，再调用 `GetProp`/`SetProp` 原生指令或运行时 API。

### VM/字节码扩展
- 引入新指令：`OpCode::GetProp/SetProp/CallMethod`
  - `GetProp rDest, rObj, symIndex`
  - `SetProp rObj, symIndex, rSrc`
  - `CallMethod rRet, rObj, symIndex, argc`
- 在 `VMState::run` 中实现：
  - 属性查找：优先对象自身属性表，否则按继承链查找；getter/setter 优先于基本存取；访问控制检查。
  - 方法调用：解析函数对象，建立新帧，将 `this` 设为 `rObj`；无闭包，不捕获外部寄存器。

### 语法解析（Parser）扩展
- 类定义：`class Foo extends Bar { var a; get b() { ... } set b(v) { ... } func m(x,y) { ... } }`
  - 在 `DeclParser` 注册 `class` 关键字；产生 `ClassDeclNode`，包含可选 `extends` 与成员列表。
- 成员属性/方法：扩展成员声明解析为 `PropertyDeclNode`（含访问标志与可选 getter/setter）、`MethodDeclNode`（参数与函数体）。
- 属性访问与方法调用表达式：沿用现有调用/成员语法，生成新字节码序列（见 VM 扩展）。

### 运行时环境
- 生命周期：所有运行时对象使用 `shared_ptr` 管理；在对象图中父→子用 `shared_ptr`，子→父与继承用 `weak_ptr` 防循环。
- 对象注册：全局符号表与实例化 API；提供 `new ClassName()` 语义（解析为构造函数或默认初始化）。

### 内存管理
- 在 `TjsValue` 的 `Object` 分支中接入引用计数，包装 `shared_ptr<RuntimeObject>`。
- 引入弱引用于：`scopeChain`、继承 `base` 与成员方法 `owner`，避免环。
- 结合现有 `src/gc` 接口，保留循环候选收集入口，但以弱引用设计为主线，先满足规范稳定性。

## 作用域链与查找
- 解析期：`IRGenerator` 维护 `scopeDepth` 与 `LocalVariable`，生成符号索引。
- 运行期：`ScopeNode` 链用于对象成员与动态 `this` 的解析入口；嵌套查找按“当前对象 → 原型/基类 → 调用帧局部 → 全局”。

## 优先级与语义细则
- 成员访问优先于同名局部变量；`obj.m()` 绑定 `this=obj`。
- `get/set` 优先级：赋值触发 `set`，读取触发 `get`；若缺省则访问基本值。
- 方法名解析按类表→基类表线性化；不支持闭包参数捕获。

## 测试与基准
- 单元测试：
  - 解析：类语法、继承、成员、getter/setter、方法调用、this 绑定
  - 运行时：属性读写、访问控制、方法调用返回、作用域查找
  - VM：新指令的执行路径与错误分支
- 性能：类方法调用与属性访问基准；与现有函数调用/全局访问对比
- 内存：对象创建/销毁压力测试，引用计数与弱引用断链验证；与 `gc_test` 集成

## 迁移策略
- 阶段化：
  1) 定义对象模型类型与 VM 指令（空实现但接口到位）
  2) 扩展 Parser/AST 节点；IRGenerator 先发射占位指令
  3) 完成 VM 指令语义与运行时对象表
  4) 补全访问控制与继承查找
  5) 全量测试与基准
- 保持向后兼容：不影响既有脚本函数与原生函数。

## 风险与缓解
- 作用域链复杂度：限定无闭包；以寄存器窗口与弱链保证简单性。
- 循环引用：系统性使用 `weak_ptr` 于父回指。
- 解析冲突：以关键字 `class/get/set/extents` 新增，避免与现有保留字冲突。

## 交付物
- 新类型与指令实现文件
- Parser/AST/IRGenerator 扩展
- VM 与运行时扩展
- 全套测试、基准与内存检测脚本

请确认该方案是否符合预期，确认后我将按阶段推进实现并提交具体改动。