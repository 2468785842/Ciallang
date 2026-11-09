# 顶层 &property成员 引用被全局读取劫持

Status: implemented

## 问题

`&o.x`（property 成员本体引用）在**顶层上下文**捕获后，全局 var 再读取时
触发 getter（typeof p 返回 void，*p 读写报 not a property）；**函数内**同形
态正常返回 Property 对象。两条路径 TAC 同构（GPropRawC），差异在存储介质：
顶层 var 是全局槽。

## 根因与证据

- GPropRawC 执行正确：`getProp` 对实例 `_props` 里的 Property 本体原样返回
  （VMState.cpp:2152-2162，诊断输出 obj typeId=3 → result typeId=17）。
- 劫持点在 **loadGlobal**（VMState.cpp loadGlobal）：全局槽 Property 值一律
  `propObjectGet` 触发 getter——该快路径只服务于**全局声明的 property**
  （`property p {}` 存于全局、读取需触发 getter），无法区分 **var p 持有的
  Property 引用值**（`&` 捕获的本体，读取应透传）。
- 区分依据：Property::thisObj。全局声明 property 的 thisObj = 全局对象；
  `&o.x` 捕获引用的 thisObj = 实例 o。

## 修复

loadGlobal 的 Property 特判增加 owner 判定：`prop->thisObj == context.global()`
才触发 getter，否则按值透传。GThis 路径（getThis → propObjectGet）不经过
全局槽，无此问题；cachedGlobal 只返回槽指针不触发 getter，同样安全。

## 验证

- `tests/tjs/oop/property-reference.tjs`：顶层/函数内 & 引用读写（setter/
  getter 计数序列 `s;g;s;g;`）+ 全局声明 property getter 不受影响。
- 全量 CTest 449/449；模糊 119/119。

## 残留边界

`&` 对普通 var 成员（非 property）仍被 DerefGet/DerefSet 拒绝（VM 仅接受
Property 对象，"not a property"）——与 TJS2 的 GPDS 符号形态语义差异，当前
`&` 协议明确只支持 property 成员；如需扩展另开批次。
