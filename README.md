# CppAura

一个轻量级的 C++ 项目，将自定义的 HTTP 服务器框架与面向聊天界面的 AI 后端结合在一起。工作区包含两个主要部分：

1. **HttpServer** – 自行编写的 HTTP/HTTPS 服务框架，提供请求/响应处理、路由、中间件、CORS 支持、会话和实用工具。
2. **AIService/ChatServer** – AI 前端的后端代码；包括请求处理器、AI 工具的实用类、语音处理、图像识别等。

---

##  仓库结构

```
CMakeLists.txt            # 构建配置
AIService/                # AI 聊天后端
  ChatServer/             # 聊天服务器实现
    include/              # 公共头文件（util、handlers）
    src/                  # 对应的源文件
    resource/             # HTML 模板和静态资源
HttpServer/               # 自定义 HTTP 框架
  include/                # 框架头文件（http、router、middleware 等）
  src/                    # 框架源文件
build/                    # CMake 生成的构建产物（忽略）
env/                      # 支持环境代码
README.md                 # 本文件
```

**AIService** 子文件夹包含由聊天前端调用的后端处理程序；每个处理程序对应一个路由（例如 `ChatLoginHandler`、`ChatSendHandler`、`AIUploadHandler`）。`AIUtil` 中的实用类封装了配置、策略模式、语音和图像处理、消息队列等。

**HttpServer** 目录定义了一个最小的 HTTP 服务器，具有：

* `HttpRequest` / `HttpResponse` 抽象
* 用于路由分发的 `Router` 和 `RouterHandler`
* 中间件链支持（CORS、会话等）
* SSL/TLS 配置类（`SslConfig`、`SslContext`）
* 会话管理与存储
* 文件、JSON 和 MySQL 实用工具

顶层的 `CMakeLists.txt` 将这两个组件编译到单个可执行文件 (`my_http_server`) 中。

---

##  项目构建

本项目使用 CMake（最低版本 3.10），目标为 C++17。外部依赖包括：

* OpenSSL
* CURL
* OpenCV
* MySQL Connector/C++
* muduo 库（用于网络）
* ONNX Runtime（用于 AI 推理）
* SimpleAmqpClient / rabbitmq（消息队列）

### 构建步骤

```bash
mkdir -p build && cd build
cmake ..                # 若库位于自定义位置，请调整路径
make -j$(nproc)
```

如果 CMake 无法找到某个库，请使用 `-D` 选项或安装缺失的软件包。

---

## 运行服务器

构建成功后，可执行文件 `my_http_server` 位于 `build` 目录中。它在配置的端口上侦听（默认在 `ChatServer/config` 或 `HttpServer` 初始化中设置），并将传入的 HTTP 请求路由到定义在 `AIService/ChatServer/src/handlers` 的相应处理程序。

`AIService/ChatServer/resource` 下的静态 HTML 页面由 `AIMenuHandler` 或 `UploadHandler` 等处理程序返回。

服务器支持普通聊天操作（登录/注册/发送）、会话跟踪、文件上传以及语音识别和图像分析等 AI 特性。

---

##  扩展代码库

* **添加新路由**：实现一个继承自通用基类的处理程序（参见现有处理程序），并在 `main.cpp` 中将其注册到路由器。
* **AI 实用工具**：`AIService/ChatServer/include/AIUtil` 下的类遵循策略/工厂模式。可以通过扩展 `AIToolRegistry` 并实现相应策略来添加新工具。
* **中间件**：扩展 `HttpServer/include/middleware` 并在服务器启动时更新链初始化。

---

##  依赖与说明

此仓库拟在 Linux 上编译。`CMakeLists.txt` 包含硬编码的包含/链接目录；请根据环境调整。

代码假定以下头文件位置可用：

* `/usr/local/include`（用于 SimpleAmqpClient 和 ONNX）
* `/usr/include/mysql-cppconn-8`
* 系统路径中的 OpenCV

OpenSSL 和 CURL 通过 `find_package` 查找。

---

