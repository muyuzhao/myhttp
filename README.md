
# MyHTTP - C 语言多线程 Web 服务器

这个项目是一个使用 C 语言实现的多线程 HTTP 服务器，旨在展示基础的 Web 服务器功能，包括处理 GET 请求、文件服务、错误处理（包括服务自定义错误页面）和多线程并发。

## 项目结构

*   `myhttp.c`: Web 服务器的核心代码。
*   `index.html`: 默认的主页文件，用于演示服务器功能。
*   `my404.html`: **定制的 404 错误页面，服务器在文件未找到时会尝试服务此页面。**

## 功能特性

### `myhttp.c` - Web 服务器

*   **基础 HTTP/1.1 支持:** 能够接收和处理 HTTP GET 请求。
*   **多线程并发:** 使用 `pthread` 库为每个传入的客户端连接创建一个独立的线程来处理请求，实现并发处理。
*   **文件服务:**
    *   将 `/` 请求映射到 `WEBROOT/index.html`。
    *   服务 `WEBROOT` 目录下请求的其他文件。
    *   **根据文件扩展名自动推断并设置 `Content-Type` 响应头（例如：`.html` 文件发送 `text/html`，`.css` 文件发送 `text/css`）。**
*   **错误处理:**
    *   **404 Not Found:** **当请求的文件不存在时，服务器会尝试返回 `WEBROOT/my404.html`。如果 `my404.html` 不存在或无法访问，则回退到生成一个简单的 HTML 错误信息。**
    *   **400 Bad Request:** 请求格式不正确，或包含目录遍历 (`..`) 尝试。
    *   **403 Forbidden:** **当请求路径指向一个目录时（而不是一个具体文件），服务器会返回此错误。**
    *   **500 Internal Server Error:** 服务器内部错误（如文件打开失败）。
    *   **501 Not Implemented:** 只支持 GET 方法，其他方法会返回此错误。
*   **安全考虑:** 包含简单的目录遍历 (`..`) 检测，防止客户端访问 `WEBROOT` 之外的文件。
*   **健壮性:** 在解析 HTTP 请求时，复制原始请求字符串以避免 `strtok_r` 对原始缓冲区的修改，确保后续操作的稳定性。
*   **可配置性:** 可以通过修改宏定义 `PORT` 和 `WEBROOT` 来调整服务器监听端口和网站根目录。
*   **日志输出:** 使用 `pthread_mutex_t` 保护标准输出，确保多线程环境下的日志输出不会混淆。

### `index.html` - 网站主页示例

*   一个模拟的搜索页面，具有 Google 风格的简洁设计。
*   包含一个搜索框和搜索按钮，以及底部导航链接。
*   使用了基本的 CSS 来美化页面布局和元素。

### `my404.html` - 定制 404 错误页面示例

*   一个具有吸引力的 404 页面设计，包含动画效果和响应式布局。
*   展示了如何使用 CSS 渐变、文本裁剪、动画来创建视觉效果。
*   **服务器现在会尝试直接服务此文件作为 404 响应。**

## 如何编译和运行

### 1. 配置 `WEBROOT`

在编译之前，请务必修改 `myhttp.c` 文件中的 `WEBROOT` 宏定义，使其指向你的 `index.html` 和 `my404.html` 所在的目录。

```c
#define WEBROOT "/home/stu/quzijie/http" // <-- 请修改为你的实际路径
```

例如，如果你的 `myhttp.c`, `index.html`, `my404.html` 都放在 `/path/to/my/website` 目录下，那么 `WEBROOT` 就应该设置为 `"/path/to/my/website"`。

### 2. 编译

使用 GCC 编译器编译 `myhttp.c`。请确保链接 `pthread` 库。

```bash
gcc -o myhttp myhttp.c -pthread
```

### 3. 运行

如果 `PORT` 设置为 80 (HTTP 标准端口)，通常需要 `root` 权限才能绑定此端口。

```bash
sudo ./myhttp
```

如果使用非特权端口（例如 8080），则不需要 `sudo`：

```bash
./myhttp
```

### 4. 测试

服务器启动后，你可以在浏览器中访问 `http://localhost/` 或 `http://127.0.0.1/` 来查看 `index.html`。

你也可以使用 `curl` 命令进行测试：

*   **访问主页 (Content-Type 应为 `text/html`):**
    ```bash
    curl -I http://localhost/ # -I 只获取头部信息
    curl http://localhost/    # 获取页面内容
    ```
*   **访问不存在的文件 (应收到 `my404.html` 的内容和 404 状态码):**
    ```bash
    curl http://localhost/nonexistent.html
    ```
*   **尝试访问目录 (应收到 403 Forbidden):**
    ```bash
    # 假设你的WEBROOT下有子目录 'css'
    curl http://localhost/css/
    ```
*   **尝试目录遍历 (会被阻止并收到 400):**
    ```bash
    curl http://localhost/../
    ```

## 注意事项

*   确保 `WEBROOT` 路径正确且服务器进程对该目录及其文件具有读取权限。
```
