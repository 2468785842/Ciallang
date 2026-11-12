# Cial SDK 声明包

本目录是编辑器使用的接口契约，不是可执行脚本。每个顶层类或命名空间单独放在
`<类型名>.d.tjs`，全局函数放在 `globals.d.tjs`。`index.json` 列出 SDK 版本、文件清单
和 SHA-256；请随服务器和运行库整体更新，避免混合不同版本的声明。

## 编写和组织项目接口

项目接口建议放在 `types/`，按模块和类型拆分；每个文件只维护一个主要公共类型。
宿主 API 使用 namespace 归属，避免把所有类型堆入全局作用域：

```tjs
/** 资源服务。 */
namespace Assets {
  /** 可复用的图像资源。 */
  class Image {
    /** 图像宽度，单位为像素。 */
    property width { getter: Integer; }
  }
  /**
   * 从资源路径读取图像。
   * @param filename 相对资源根目录的路径。
   * @returns 新图像资源。
   * @throws 文件不存在或图像格式无效。
   * @example var image = Assets.loadImage("images/title.png");
   */
  function loadImage(filename: String): Assets.Image;
}
```

## 类型与文档约定

- 参数必须具名，优先写 Integer、Real、Boolean、String、Octet、Object 或实际宿主类型。
- 使用 `A | B` 表达有限联合类型，例如 `String | RegExp`；`Name[]` 表达元素类型明确的数组。
- 默认值表达可选参数；TJS 的 `values: any*` 对应具名变长参数，星号表示收拢而非未知类型。
- `any` 仅用于真正接受或返回任意脚本值的位置。Array 元素可混合类型，Exception.message
  按原值保存，Scripts 的求值结果由脚本决定；这些位置不能假称为固定 String/Integer。
- 注释使用 `/** */`，包含行为摘要、`@param`、`@returns`、`@throws` 和可运行的 `@example`。
- 函数名、参数名、默认值、返回类型、属性读写性必须与实现一致；声明不改变运行时检查。
- 这是 Ciallang 的 `.d.tjs` 契约语法；支持命名空间、限定类型和联合类型，尚不支持 TypeScript
  的 import/export、泛型或完整 Java 类型系统。

LSP 自动读取本目录与项目 `types/` 中的声明。自动扫描排除 build/out 等构建目录；
外部宿主 SDK 可通过 `ciallang.declarationFiles` 显式指定。不要把本目录复制到工作区多处。
