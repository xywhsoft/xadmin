# 插件系统 V2 更新说明

## 更新概览

本次更新为插件系统添加了两个重要功能：

1. **依赖管理** - 支持插件间依赖关系和自动加载顺序
2. **命名空间隔离** - 避免插件符号冲突

---

## 1. 依赖管理

### 1.1 功能特性

- ✅ 自动解析插件依赖关系
- ✅ 使用拓扑排序确定加载顺序
- ✅ 支持版本范围检查
- ✅ 检测循环依赖并报错
- ✅ 兼容旧格式（纯字符串数组）

### 1.2 配置方式

#### 方式一：对象格式（推荐）
```json
{
    "name": "my_plugin",
    "dependencies": [
        {
            "plugin": "base_plugin",
            "minVersion": "1.0.0",
            "maxVersion": "2.0.0"
        }
    ]
}
```

#### 方式二：字符串格式（向后兼容）
```json
{
    "name": "my_plugin",
    "dependencies": ["base_plugin"]
}
```

### 1.3 版本比较规则

系统使用语义化版本比较 `major.minor.patch`：

- `minVersion` - 最小版本要求（包含）
- `maxVersion` - 最大版本限制（包含）

**示例：**
```json
{
    "dependencies": [
        {"plugin": "base", "minVersion": "1.2.0"},  // >= 1.2.0
        {"plugin": "util", "maxVersion": "2.0.0"}   // <= 2.0.0
    ]
}
```

### 1.4 加载顺序

系统会自动根据依赖关系确定加载顺序：

```
插件A (无依赖)
  ↓
插件B (依赖 A)
  ↓
插件C (依赖 B)
```

### 1.5 错误处理

如果出现以下情况，系统会拒绝加载插件：

1. **依赖不存在** - 依赖的插件未安装
2. **版本不匹配** - 依赖的插件版本不满足要求
3. **循环依赖** - 插件间存在循环依赖

错误日志示例：
```
[Plugin] ERROR: Plugin 'dependent' depends on missing plugin 'missing_plugin'
[Plugin] ERROR: Plugin 'dependent' version 1.0.0 does not meet requirements of 'parent'
[Plugin] ERROR: Circular dependency detected!
```

---

## 2. 命名空间隔离

### 2.1 功能特性

- ✅ 自动为所有插件符号添加前缀
- ✅ 避免不同插件的函数名冲突
- ✅ 提供便捷宏定义简化开发

### 2.2 工作原理

系统会在编译插件代码时自动注入前缀定义：

```c
#define PLUGIN_NS(name) _plugin_{plugin_name}_##name
#define PLUGIN_API(name) PLUGIN_NS(API_##name)
#define PLUGIN_FUNC(name) PLUGIN_NS(name)
```

### 2.3 使用方法

#### 推荐方式：使用宏定义

```c
// 路由处理函数
void PLUGIN_API(MyHandler)(void* s, void* h,
                           struct mg_connection* c, struct mg_http_message* hm)
{
    ctx->SendJson(c, 200, "{\"result\":true}", 0);
}

// 内部函数
void PLUGIN_FUNC(DoSomething)()
{
    // 内部逻辑
}

// 导出函数
int PLUGIN_FUNC(Calculate)(int a, int b)
{
    return a + b;
}

// 插件入口
void Plugin_my_plugin_Init()
{
    ctx->AddRoute("/api/test", PLUGIN_API(MyHandler), FALSE, FALSE, 0, 0);
}

void Plugin_my_plugin_Unit()
{
    ctx->RemoveRoute("/api/test");
}
```

#### 展开后的实际符号名：

| 宏调用 | 展开结果 |
|--------|----------|
| `PLUGIN_API(MyHandler)` | `_plugin_my_plugin_API_MyHandler` |
| `PLUGIN_FUNC(Calculate)` | `_plugin_my_plugin_Calculate` |
| `Plugin_my_plugin_Init()` | `_plugin_my_plugin_Plugin_my_plugin_Init` |

### 2.4 特殊说明

以下函数名保持不变（系统自动处理）：

- `Plugin_SetGlobalData` - 系统会自动添加前缀
- `Plugin_{plugin_name}_Init` - 系统会自动添加前缀
- `Plugin_{plugin_name}_Unit` - 系统会自动添加前缀

---

## 3. 完整示例

### 3.1 基础插件 (base_plugin)

**config.json:**
```json
{
    "name": "base_plugin",
    "version": "1.0.0",
    "dependencies": [],
    "exports": ["Calculate", "GetData"]
}
```

**main.c:**
```c
#include "plugin.h"

PluginContext* ctx;

// 导出函数
int PLUGIN_FUNC(Calculate)(int a, int b)
{
    return a + b;
}

// API 路由
void PLUGIN_API(GetInfo)(void* s, void* h,
                         struct mg_connection* c, struct mg_http_message* hm)
{
    ctx->SendJson(c, 200, "{\"result\":true}", 0);
}

void Plugin_base_plugin_Init()
{
    ctx->AddRoute("/api/base/info", PLUGIN_API(GetInfo), FALSE, FALSE, 0, 0);
    ctx->SetPluginExport("base_plugin", "Calculate", PLUGIN_FUNC(Calculate));
}

void Plugin_base_plugin_Unit()
{
    ctx->RemoveRoute("/api/base/info");
}
```

### 3.2 依赖插件 (dependent_plugin)

**config.json:**
```json
{
    "name": "dependent_plugin",
    "version": "1.0.0",
    "dependencies": [
        {"plugin": "base_plugin", "minVersion": "1.0.0"}
    ]
}
```

**main.c:**
```c
#include "plugin.h"

PluginContext* ctx;

// 定义函数指针类型
typedef int (*FnCalculate)(int, int);
FnCalculate g_fnCalculate = NULL;

// API 路由
void PLUGIN_API(UseBase)(void* s, void* h,
                          struct mg_connection* c, struct mg_http_message* hm)
{
    if ( g_fnCalculate ) {
        int result = g_fnCalculate(10, 20);
        // ...
    }
}

void Plugin_dependent_plugin_Init()
{
    // 系统会确保 base_plugin 已加载
    ctx->AddRoute("/api/dependent/use", PLUGIN_API(UseBase), FALSE, FALSE, 0, 0);
}

void Plugin_dependent_plugin_Unit()
{
    ctx->RemoveRoute("/api/dependent/use");
}
```

---

## 4. 迁移指南

### 4.1 从 V1 迁移到 V2

**需要修改的地方：**

1. **函数定义** - 使用 `PLUGIN_API` 和 `PLUGIN_FUNC` 宏

   ```c
   // V1 旧写法
   void API_MyHandler(...) { ... }

   // V2 新写法
   void PLUGIN_API(MyHandler)(...) { ... }
   ```

2. **config.json** - 可选：升级依赖格式

   ```json
   // V1 旧格式（仍然支持）
   "dependencies": ["base_plugin"]

   // V2 新格式（推荐）
   "dependencies": [
       {"plugin": "base_plugin", "minVersion": "1.0.0"}
   ]
   ```

### 4.2 兼容性

- ✅ 旧格式的 `dependencies` 仍然支持
- ✅ 未使用宏的插件仍然可以正常运行（但不建议）
- ⚠️ 不同插件使用相同函数名会导致冲突（请使用宏）

---

## 5. 常见问题

### Q1: 为什么我的插件加载失败？

检查日志中的错误信息：
- 依赖插件是否已安装？
- 版本是否满足要求？
- 是否存在循环依赖？

### Q2: 如何调试加载顺序？

查看启动日志：
```
[Plugin] Validating dependencies...
[Plugin] Loading enabled plugins...
[Plugin] Plugin loaded: base_plugin
[Plugin] Plugin loaded: dependent_plugin
```

### Q3: 符号前缀会影响性能吗？

不会。前缀在编译时自动添加，对运行时性能无影响。

### Q4: 我可以禁用命名空间吗？

不推荐。禁用命名空间会导致不同插件的符号可能冲突。

---

## 6. 技术细节

### 6.1 拓扑排序算法

系统使用 **Kahn 算法** 进行拓扑排序：

1. 计算每个节点的入度
2. 将入度为0的节点加入队列
3. 依次取出节点并减少依赖节点的入度
4. 检测循环依赖（节点数 < 总数）

### 6.2 符号前缀实现

系统在编译前预处理代码：

```c
// 原始代码
void PLUGIN_API(Test)(...) { ... }

// 自动展开为
#define PLUGIN_API(name) _plugin_hello_API_##name
void _plugin_hello_API_Test(...) { ... }
```

---

## 7. 未来计划

- [ ] 热重载功能
- [ ] 数据库迁移机制
- [ ] 钩子系统
- [ ] 版本兼容性检查
- [ ] 插件市场支持

---

## 8. 更新日志

### V2.0.0 (2026-02-03)

**新增：**
- ✨ 依赖管理系统
- ✨ 命名空间隔离
- ✨ 版本范围检查
- ✨ 拓扑排序加载
- ✨ 循环依赖检测

**改进：**
- 🔧 优化加载顺序算法
- 🔧 增强错误提示信息

**兼容性：**
- ✅ 向后兼容 V1 格式
- ✅ 旧插件无需修改即可运行

---

如有问题或建议，欢迎反馈！
