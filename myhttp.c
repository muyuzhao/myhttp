#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>

// ========== 配置区 ==========
#define PORT 80
#define WEBROOT "/home/stu/quzijie/http" // 请修改为您的网站文件根目录
#define MAX_HEADER_SIZE 1024
#define MAX_PATH_SIZE 256

// ========== 全局资源 ==========
pthread_mutex_t stdout_lock; // 用于保护标准输出，防止日志交错

/**
 * @brief 创建并初始化服务器监听套接字
 * @return 成功返回监听套接字描述符，失败返回-1
 */
int create_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    // 设置地址重用，防止服务器重启时bind失败
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = INADDR_ANY; // 监听所有网络接口

    if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 10) == -1) { // listen队列大小设为10
        perror("listen");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/**
 * @brief 发送标准的HTTP错误响应
 * @param client_sock 客户端套接字
 * @param status_code HTTP状态码
 * @param status_message 状态消息 (如 "Not Found")
 */
void send_error_response(int client_sock, int status_code, const char* status_message) {
    char header[MAX_HEADER_SIZE];
    char body[MAX_PATH_SIZE];

    // 构造HTML响应体
    sprintf(body, "<html><head><title>%d %s</title></head><body><h1>%d %s</h1></body></html>", 
            status_code, status_message, status_code, status_message);

    // 构造HTTP响应头
    sprintf(header, "HTTP/1.1 %d %s\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: %ld\r\n"
                    "Server: C-Thread-Server/1.1\r\n"
                    "Connection: close\r\n\r\n", 
            status_code, status_message, strlen(body));

    send(client_sock, header, strlen(header), 0);
    send(client_sock, body, strlen(body), 0);
}

/**
 * @brief 解析HTTP请求，提取方法和文件名
 * @param request_buffer 包含HTTP请求的缓冲区
 * @param method 用于存储请求方法的指针
 * @param filename 用于存储文件名的指针
 * @return 0表示成功, -1表示请求格式错误
 */
int parse_http_request(char* request_buffer, char** method, char** filename) {
    char* saveptr;
    *method = strtok_r(request_buffer, " \t", &saveptr);
    *filename = strtok_r(NULL, " \t", &saveptr);
    
    if (*method == NULL || *filename == NULL) {
        return -1; // 格式错误
    }
    return 0;
}

/**
 * @brief 工作线程函数，处理单个客户端的完整请求
 * @param arg 指向在堆上分配的客户端套接字描述符的指针
 */
void* work_fun(void* arg) {
    int c = *((int*)arg);
    free(arg); // 关键：立即释放主线程传递过来的堆内存，防止泄漏

    char request_buffer[MAX_HEADER_SIZE] = {0};
    int n = recv(c, request_buffer, sizeof(request_buffer) - 1, 0);

    // 加锁保护printf，确保日志完整性
    pthread_mutex_lock(&stdout_lock);
    printf("----------\nThread %lu processing client %d\n", pthread_self(), c);
    if (n <= 0) {
        if (n < 0) perror("recv");
        printf("Client %d disconnected or error.\n", c);
        pthread_mutex_unlock(&stdout_lock);
        close(c);
        return NULL;
    }
    printf("Received Request from client %d:\n%s\n", c, request_buffer);
    pthread_mutex_unlock(&stdout_lock);

    char* method, *filename;
    if (parse_http_request(request_buffer, &method, &filename) != 0) {
        send_error_response(c, 400, "Bad Request");
        close(c);
        return NULL;
    }

    // 只支持GET方法
    if (strcmp(method, "GET") != 0) {
        send_error_response(c, 501, "Not Implemented");
        close(c);
        return NULL;
    }

    char full_path[MAX_PATH_SIZE];
    // 处理根目录请求
    if (strcmp(filename, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/index.html", WEBROOT);
    } else {
        snprintf(full_path, sizeof(full_path), "%s%s", WEBROOT, filename);
    }
    
    // 防止目录遍历攻击
    if (strstr(full_path, "..") != NULL) {
        send_error_response(c, 400, "Bad Request");
        close(c);
        return NULL;
    }

    struct stat file_stat;
    if (stat(full_path, &file_stat) < 0) {
        send_error_response(c, 404, "Not Found");
        close(c);
        return NULL;
    }

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        send_error_response(c, 500, "Internal Server Error");
        close(c);
        return NULL;
    }

    // 构造200 OK响应
    char header[MAX_HEADER_SIZE];
    sprintf(header, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n" // 可根据文件类型修改
                    "Content-Length: %ld\r\n"
                    "Server: C-Thread-Server/1.1\r\n\r\n",
            file_stat.st_size);
    
    send(c, header, strlen(header), 0);

    // 使用循环发送文件内容
    char data_buffer[1024];
    ssize_t num_read;
    while ((num_read = read(fd, data_buffer, sizeof(data_buffer))) > 0) {
        if (send(c, data_buffer, num_read, 0) < 0) {
            // 发送失败，客户端可能已关闭连接
            break;
        }
    }

    close(fd);
    close(c);
    
    pthread_mutex_lock(&stdout_lock);
    printf("Thread %lu finished processing client %d.\n----------\n\n", pthread_self(), c);
    pthread_mutex_unlock(&stdout_lock);
    
    return NULL;
}

int main() {
    int sockfd = create_socket();
    assert(sockfd != -1);

    // 初始化互斥锁
    if (pthread_mutex_init(&stdout_lock, NULL) != 0) {
        perror("mutex init");
        return 1;
    }

    printf("Server started on port %d, web root is %s\n", PORT, WEBROOT);
    
    while (1) {
        printf("Waiting for new client...\n");
        
        // 为传递给线程的套接字描述符在堆上分配内存
        int* c_ptr = (int*)malloc(sizeof(int));
        if (c_ptr == NULL) {
            perror("malloc");
            continue; // 内存不足，跳过此次请求
        }

        *c_ptr = accept(sockfd, NULL, NULL);
        if (*c_ptr < 0) {
            perror("accept");
            free(c_ptr); // 接受失败，释放内存
            continue;
        }
        
        pthread_mutex_lock(&stdout_lock);
        printf("Accepted a new client, socket fd: %d\n", *c_ptr);
        pthread_mutex_unlock(&stdout_lock);

        pthread_t id;
        // 关键：将堆上独立的内存地址c_ptr传递给线程
        if (pthread_create(&id, NULL, work_fun, c_ptr) != 0) {
            perror("pthread_create");
            // 线程创建失败，必须关闭套接字并释放内存，防止资源泄漏
            close(*c_ptr);
            free(c_ptr);
        }
        // 分离线程，使其结束后自动回收资源，避免僵尸线程
        pthread_detach(id);
    }

    close(sockfd);
    // 销毁互斥锁
    pthread_mutex_destroy(&stdout_lock);
    return 0;
}
