# 胡胡算番 🀄

国标麻将（中国麻将竞赛规则 / MCR）算番器 Android App。完全离线运行，点牌即算，支持全部 81 种番型。

> 仅作算番学习与线下娱乐辅助使用，不含任何联网对战、金币或博彩功能。

## 功能

- **可视化点牌**：牌池按 万 / 条 / 饼 / 字 分四行展示，点击添加手牌
- **一键副露**：点「吃 / 碰 / 暗杠 / 明杠」后再点牌池一张牌，自动组成副露组
  - 吃：以点击的牌为顺子最小张（点 3 万 = 吃 234 万）
  - 暗杠与明杠分开处理，正确影响 暗刻 / 门前清 / 双暗杠 等番型判定
- **听牌提示**：手牌到 13 张时自动计算所有听牌，在顶部显示每张听牌的番数，并在牌池中红色高亮
- **和牌张标记**：长按（手机）或右键（桌面预览）手牌可标记和牌张
- **完整算番**：国标 81 番种，自动求最优拆解，不漏番、不重番
- **场况设置**：和牌方式（自摸 / 点和 / 杠上开花 / 海底 / 绝张等）、圈风、门风、花牌数
- **计分明细**：番数转基本分，庄家自摸 / 闲家自摸 / 点和的分摊
- **完全离线**：算番引擎打包进 APK，无需服务器、无网络权限需求

## 下载

前往 [Releases](../../releases) 页面下载最新 APK，安装到 Android 手机即可使用（需允许安装未知来源应用）。

## 从源码构建

### 环境要求

- Node.js ≥ 18
- JDK 21
- Android SDK（platform-tools、platforms;android-36、build-tools）

### 步骤

```bash
# 1. 安装依赖
cd app
npm install

# 2.（可选）重新打包算番引擎 bundle
#    仓库已包含构建好的 www/gb-bundle.js，此步仅在你改动 entry.js 或升级依赖时需要
npx esbuild entry.js --bundle --platform=browser --format=iife --global-name=GBM --minify --outfile=www/gb-bundle.js

# 3. 同步 Web 资源到 Android 工程
npx cap sync android

# 4. 构建 APK
cd android
./gradlew assembleDebug

# 产物位置
# android/app/build/outputs/apk/debug/app-debug.apk
```

> 修改 `app/www/` 下的前端代码后，重复步骤 3、4 即可。

### 网络受限环境（中国大陆）

本项目在 Gradle wrapper 与 build.gradle 中已配置腾讯云 / 阿里云镜像源；若仍遇下载问题，可将 [`docs/gradle-mirror-init.gradle`](docs/gradle-mirror-init.gradle) 复制到 `~/.gradle/init.d/init.gradle` 启用全局镜像。

## 技术架构

```
┌─────────────────────────────────────────┐
│  Android APK (Capacitor 8 打包)          │
│  └── WebView                             │
│      ├── index.html     单文件前端 UI     │
│      ├── gb-bundle.js   算番引擎 (esbuild)│
│      └── assets/tiles/  34 张麻将牌 SVG   │
└─────────────────────────────────────────┘
```

- **前端**：无框架单文件 HTML/CSS/JS，麻将牌使用 [riichi-mahjong-tiles](https://github.com/FluffyStuff/riichi-mahjong-tiles)（CC0）SVG 素材
- **算番**：[gb-mahjong-js](https://github.com/tziakcha-stats/gb-mahjong-js)（MIT）纯 JS 实现，浏览器内直接运算，离线可用
- **打包**：[Capacitor](https://capacitorjs.com/)（MIT）生成 Android 工程

## 目录结构

```
├── app/                    # Android App（主项目）
│   ├── www/                # Web 前端资源
│   │   ├── index.html      # 全部 UI 与交互逻辑
│   │   ├── gb-bundle.js    # 算番引擎（由 entry.js 打包生成）
│   │   └── assets/tiles/   # 34 张麻将牌 SVG
│   ├── android/            # Capacitor 生成的 Android 工程
│   ├── entry.js            # 算番引擎打包入口
│   └── capacitor.config.json
├── demo/                   # 早期 Web 版 demo（Python 本地服务 + C++ 算番）
├── LICENSE                 # 本项目许可证 (MIT)
└── THIRD_PARTY_LICENSES.md # 第三方组件许可证声明
```

## 已知限制

- 副露供牌来源默认按「上家」处理（影响极少数番型的边缘情况）
- 加杠（碰后补杠）暂未与明杠区分
- 花牌以数字选择方式输入，未做花牌可视化

## 许可证

本项目基于 [MIT License](LICENSE) 开源。

所使用的第三方组件许可证详见 [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)。
