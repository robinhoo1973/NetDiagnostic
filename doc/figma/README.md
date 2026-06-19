# Figma 设计文件导入说明

## 文件列表

| 文件 | 内容 | 尺寸 |
|------|------|------|
| `00-design-system.svg` | 设计系统：调色板、排版、间距 | 900×500 |
| `01-diagnostics-desktop-wide.svg` | 诊断页 — 桌面宽屏 (≥600px) | 1100×750 |
| `02-diagnostics-phone-narrow.svg` | 诊断页 — 手机窄屏 (<600px) | 375×812 |
| `03-dashboard-desktop.svg` | Dashboard 仪表盘 | 1100×700 |
| `04-config-desktop.svg` | 配置页 (TabBar + 开关列表) | 1100×700 |
| `05-simulator-desktop.svg` | 模拟器页面 (设备框架) | 1100×700 |

## 导入 Figma 的方法

### 方法 1：拖放导入 (推荐)
1. 打开 Figma 项目
2. 将 `.svg` 文件直接拖入 Figma 画布
3. Figma 会自动将 SVG 转换为可编辑的 Figma 图层
4. 所有颜色、文字、形状都会保留

### 方法 2：File > Import
1. Figma 菜单 → File → Import
2. 选择 `.svg` 文件
3. 导入后会作为独立的 Frame

### 方法 3：Design Tokens 插件
1. 安装 "Design Tokens" Figma 插件
2. 使用 `doc/design-tokens.json` 文件
3. 可以批量导入所有颜色、排版、间距 token

## 设计系统总结

| Token | Hex | 用途 |
|-------|-----|------|
| bgDark | #1E1E2E | 页面背景 |
| bgSidebar | #252538 | 侧边栏 |
| bgCard | #16213E | 卡片容器 |
| AppBar | #1A1A2E | 顶栏 |
| accentBlue | #0078D4 | 主色调 (按钮/激活态) |
| cyan | #00BCD4 | 运行状态 |
| passGreen | #4ADE80 | 通过 |
| warnYellow | #FACC15 | 警告 |
| failRed | #EF4444 | 失败 |
| skipGray | #888888 | 跳过 |

- **字体**: JetBrains Mono (等宽)
- **图标**: Material Icons (Phosphor Icons 备选)
- **断点**: ≥600px 宽屏 / <600px 窄屏
- **侧边栏**: 260px (仅宽屏显示)
