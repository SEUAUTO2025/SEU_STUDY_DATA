#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define USERS_FILE "users.txt"
#define FRIENDS_FILE "friends.txt"

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    char username[256];
    int logged_in;
} client_info_t;

// 全局客户端列表和互斥锁
client_info_t *g_clients[MAX_CLIENTS];
pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void add_client(client_info_t *cl) {
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_clients[i]) {
            g_clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
}

void remove_client(client_info_t *cl) {
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i] == cl) {
            g_clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
}

int find_client_socket(const char *username) {
    int sockfd = -1;
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i] && g_clients[i]->logged_in && strcmp(g_clients[i]->username, username) == 0) {
            sockfd = g_clients[i]->sockfd;
            break;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return sockfd;
}

// 计算文件MD5值
int get_md5_by_cmd(const char *filename, char *result, size_t result_size) {
    char cmd[256];
    FILE *fp;

    // 构建命令字符串
    snprintf(cmd, sizeof(cmd), "md5sum %s | awk '{print $1}'", filename);

    // 执行命令并获取输出
    if ((fp = popen(cmd, "r")) == NULL) {
        perror("popen failed");
        return -1;
    }

    // 读取命令输出
    if (fgets(result, result_size, fp) == NULL) {
        pclose(fp);
        return -1;
    }

    // 移除末尾的换行符
    result[strcspn(result, "\n")] = '\0';

    pclose(fp);
    return 0;
}

// 检查用户是否存在
int user_exists(const char *username) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) return 0;
    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        char existing_user[256];
        sscanf(line, "%s", existing_user);
        if (strcmp(existing_user, username) == 0) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

// 添加用户
void register_user(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "a");
    if (fp) {
        fprintf(fp, "%s %s\n", username, password);
        fclose(fp);
    }
}

// 登录验证
int check_login(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) return 0;
    char line[512];
    int success = 0;
    while (fgets(line, sizeof(line), fp)) {
        char existing_user[256], existing_pass[256];
        sscanf(line, "%s %s", existing_user, existing_pass);
        if (strcmp(existing_user, username) == 0 && strcmp(existing_pass, password) == 0) {
            success = 1;
            break;
        }
    }
    fclose(fp);
    return success;
}

// 修改密码
int change_password(const char *username, const char *old_pass, const char *new_pass) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) return -1;  // 文件无法打开

    FILE *temp_fp = fopen("users.tmp", "w");
    if (!temp_fp) {
        fclose(fp);
        return -1;
    }

    char line[512];
    int found = 0;
    int success = 0;
    while (fgets(line, sizeof(line), fp)) {
        char u[256], p[256];
        sscanf(line, "%s %s", u, p);
        if (strcmp(u, username) == 0) {
            found = 1;
            if (strcmp(p, old_pass) == 0) {
                fprintf(temp_fp, "%s %s\n", username, new_pass);
                success = 1;
            } else {
                fprintf(temp_fp, "%s", line);  // 旧密码错误，写回原行
            }
        } else {
            fprintf(temp_fp, "%s", line);
        }
    }

    fclose(fp);
    fclose(temp_fp);

    if (found && success) {
        remove(USERS_FILE);
        rename("users.tmp", USERS_FILE);
        return 1;  // 成功
    } else {
        remove("users.tmp");
        if (!found) return 0;  // 用户不存在
        return -2;             // 密码错误
    }
}

// 检查是否是好友
int are_friends(const char *user1, const char *user2) {
    FILE *fp = fopen(FRIENDS_FILE, "r");
    if (!fp) return 0;
    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        char u1[256], u2[256];
        sscanf(line, "%s %s", u1, u2);
        if ((strcmp(u1, user1) == 0 && strcmp(u2, user2) == 0) ||
            (strcmp(u1, user2) == 0 && strcmp(u2, user1) == 0)) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

// 添加好友
void add_friend(const char *user1, const char *user2) {
    if (are_friends(user1, user2)) return;
    FILE *fp = fopen(FRIENDS_FILE, "a");
    if (fp) {
        fprintf(fp, "%s %s\n", user1, user2);
        fclose(fp);
    }
}

// 删除好友
void remove_friend(const char *user1, const char *user2) {
    FILE *fp = fopen(FRIENDS_FILE, "r");
    if (!fp) return;
    FILE *temp_fp = fopen("friends.tmp", "w");
    if (!temp_fp) {
        fclose(fp);
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char u1[256], u2[256];
        sscanf(line, "%s %s", u1, u2);
        if (!((strcmp(u1, user1) == 0 && strcmp(u2, user2) == 0) ||
              (strcmp(u1, user2) == 0 && strcmp(u2, user1) == 0))) {
            fprintf(temp_fp, "%s", line);
        }
    }
    fclose(fp);
    fclose(temp_fp);
    remove(FRIENDS_FILE);
    rename("friends.tmp", FRIENDS_FILE);
}

void *handle_client(void *arg) {
    client_info_t *client = (client_info_t *)arg;
    char buffer[BUFFER_SIZE];
    int recv_len;

    printf("New client connected: %s:%d\n", inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));

    while ((recv_len = recv(client->sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[recv_len] = '\0';

        // Replace '$' with '\0'
        for (int i = 0; i < recv_len; i++) {
            if (buffer[i] == '$') {
                buffer[i] = '\0';
            }
        }

        // 解析命令和参数, 以\0分隔
        char *args[10];
        int arg_count = 0;
        args[0] = buffer;
        arg_count++;
        for (int i = 0; i < recv_len; i++) {
            if (buffer[i] == '\0') {
                if (arg_count < 10) {
                    args[arg_count++] = &buffer[i + 1];
                }
            }
        }
        char *command = args[0];
        printf("Received command: %s, arg_count: %d\n", command, arg_count);


        // 注册
        if (strcmp(command, "REG") == 0 && arg_count >= 3) {
            char *username = args[1];
            char *password = args[2];
            if (user_exists(username)) {
                const char *msg = "FAIL$User already exists";
                send(client->sockfd, msg, strlen(msg), 0);
            } else {
                register_user(username, password);
                const char *msg = "OK$Registration successful";
                send(client->sockfd, msg, strlen(msg), 0);
            }
        }
        // 登录
        else if (strcmp(command, "LOGIN") == 0 && arg_count >= 3) {
            char *username = args[1];
            char *password = args[2];
            if (check_login(username, password)) {
                client->logged_in = 1;
                strcpy(client->username, username);
                const char *msg = "OK$Login successful";
                send(client->sockfd, msg, strlen(msg), 0);
                printf("User '%s' logged in from %s:%d\n", username, inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));
            } else {
                const char *msg = "FAIL$Invalid username or password";
                send(client->sockfd, msg, strlen(msg), 0);
            }
        }
        // 修改密码
        else if (strcmp(command, "CHGPWD") == 0 && arg_count >= 3) {
            if (!client->logged_in) {
                const char *msg = "FAIL$Not logged in";
                send(client->sockfd, msg, strlen(msg), 0);
                continue;
            }
            char *old_pass = args[1];
            char *new_pass = args[2];
            int result = change_password(client->username, old_pass, new_pass);
            if (result == 1) {
                const char *msg = "OK$Password changed successfully";
                send(client->sockfd, msg, strlen(msg), 0);
            } else if (result == -2) {
                const char *msg = "FAIL$Incorrect old password";
                send(client->sockfd, msg, strlen(msg), 0);
            } else {
                const char *msg = "FAIL$Failed to change password";
                send(client->sockfd, msg, strlen(msg), 0);
            }
        }
        // 添加好友
        else if (strcmp(command, "ADDFRIEND") == 0 && arg_count >= 2) {
             if (!client->logged_in) {
                const char *msg = "FAIL$Not logged in";
                send(client->sockfd, msg, strlen(msg), 0);
                continue;
            }
            char *friend_name = args[1];
            if (!user_exists(friend_name)) {
                const char *msg = "FAIL$Friend does not exist";
                send(client->sockfd, msg, strlen(msg), 0);
            } else if (strcmp(client->username, friend_name) == 0) {
                 const char *msg = "FAIL$Cannot add yourself";
                send(client->sockfd, msg, strlen(msg), 0);
            }
            else {
                add_friend(client->username, friend_name);
                const char *msg = "OK$Friend added successfully";
                send(client->sockfd, msg, strlen(msg), 0);
            }
        }
        // 删除好友
        else if (strcmp(command, "DELFRIEND") == 0 && arg_count >= 2) {
             if (!client->logged_in) {
                const char *msg = "FAIL$Not logged in";
                send(client->sockfd, msg, strlen(msg), 0);
                continue;
            }
            char *friend_name = args[1];
            remove_friend(client->username, friend_name);
            const char *msg = "OK$Friend removed successfully";
            send(client->sockfd, msg, strlen(msg), 0);
        }
        // 发送消息
        else if (strcmp(command, "MSG") == 0 && arg_count >= 3) {
            if (!client->logged_in) {
                const char *msg = "FAIL$Not logged in";
                send(client->sockfd, msg, strlen(msg), 0);
                continue;
            }
            char *recipient = args[1];
            char *message = args[2];
            if (!are_friends(client->username, recipient)) {
                 const char *msg = "FAIL$You are not friends with this user";
                send(client->sockfd, msg, strlen(msg), 0);
                 continue;
            }
            int recipient_sock = find_client_socket(recipient);
            if (recipient_sock != -1) {
                char msg_packet[BUFFER_SIZE];
                int len = snprintf(msg_packet, sizeof(msg_packet), "MSG$%s$%s", client->username, message);
                send(recipient_sock, msg_packet, len, 0);
            } else {
                const char *msg = "FAIL$User is not online";
                send(client->sockfd, msg, strlen(msg), 0);
            }
        } else {
             const char *msg = "FAIL$Unknown command or wrong parameters";
            send(client->sockfd, msg, strlen(msg), 0);
        }
    }

    if (recv_len == 0) {
        printf("Client %s:%d disconnected\n", inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));
    } else if (recv_len == -1) {
        perror("recv error");
    }

    remove_client(client);
    close(client->sockfd);
    free(client);
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 设置SO_REUSEADDR避免端口占用
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(2333);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server started, waiting for connections...\n");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        client_info_t *client = malloc(sizeof(client_info_t));
        if (!client) {
            perror("malloc");
            close(client_fd);
            continue;
        }
        client->sockfd = client_fd;
        client->addr = client_addr;
        client->logged_in = 0;
        memset(client->username, 0, sizeof(client->username));

        add_client(client);

        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) != 0) {
            perror("pthread_create");
            remove_client(client);
            close(client_fd);
            free(client);
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
