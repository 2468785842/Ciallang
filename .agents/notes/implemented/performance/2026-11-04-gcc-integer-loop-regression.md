# 语义对齐批次的 GCC 整数循环回退

Status: implemented（2026-11-10 归因并消除）

## 问题

语义/SDK 对齐批次后，GCC 构建的全局整数循环出现约 +10% 同机 Release 回退，
生成 TAC 一致，尚未归因；正式发布前需复核。

## 已知事实与证据

- 测量与基线见[性能记录](../../../../doc/performance.md#2026-11-01-语义对齐与-sdk-基础批次)；
  同机 Release 对照约 +10%，TAC 一致说明差异在代码生成之后的层
  （本机代码布局、运行期热路径布局或宿主优化交互）。

## 归因与修复（2026-11-10，WSL2 Ubuntu 26.04，g++ 15.2.0）

- **复现**：基线 04dbd2e vs HEAD（7ab81e3），loop_int 交替 11 轮两轮独立测量：
  +9.78% / +7.76%（原 GCC 14.2 记录 +10%，非单一版本现象、非偶然抖动）。
  GCC 15 对旧代码新增 format-truncation 分析警告，仅以
  `-Wno-error=format-truncation` 放宽，不影响代码生成。
- **二分**：`git bisect run`（72 提交，交替 7 轮中位数 ≥ +5% 判 bad）定位首坏
  提交 **`b9ae0db`**（该点 +19.4%，后续批次回收到 +8~10%）。
- **根因**：b9ae0db 给 GThis/DThis 全局快路径守卫加入
  `cur->thisObj.asObject().value() == context.global()`；`Context::global()`
  定义在 Context.cpp（pimpl 解引用），无 LTO 不可内联——顶层脚本的 thisObj
  即全局对象，短路不生效，每次全局读写一次真实调用（loop_int 每轮约 6 次）。
  属真实路径退化，不是布局抖动；原记录"仅主循环布局变化"的判断不成立。
- **修复**：VMState 缓存 `_globalObject` 指针（构造期取一次），两处守卫与
  尾递归判定改用缓存值（`src/vm/VMState.hpp/.cpp`）。
- **结果**：修复后 loop_int 相对 04dbd2e 基线 **−3.95% / −2.35%**——回归
  消除，且此前被掩盖的后续批次优化显现。Windows Release 全量 CTest 249/249、
  WSL Linux Debug 449/449。原始样本与二进制哈希见[测量报告](../../../reports/2026-11-10-perf-attribution.json)。

## 完成条件

归因结论回写本 note 与性能记录：真实退化需消除；
纯布局抖动需两次以上测量确认并记录接受理由。——**真实退化已消除**（见上节）。

## 风险或备选方案

代码布局类回退对编译器版本敏感，结论须注明 GCC 版本与测量环境；
不接受以重排无关代码"碰运气"消除回退。
