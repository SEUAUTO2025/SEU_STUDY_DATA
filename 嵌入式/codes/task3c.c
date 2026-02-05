#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

char g_username[256] = {0};
int g_logged_in = 0;

void get_input(const char *prompt, char *buffer, size_t size) {
    printf("%s", prompt);
    fgets(buffer, size, stdin);
    buffer[strcspn(buffer, "\n")] = 0;  // 移除换行符
}

// 处理服务器响应并返回状态 (1 for OK, 0 for FAIL)
int handle_server_response(int cfd) {
    char buffer[1024];
    int len = recv(cfd, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0) {
        printf("Server disconnected or error occurred.\n");
        return 0;
    }
    buffer[len] = '\0';

    // Replace '$' with '\0'
    for (int i = 0; i < len; i++) {
        if (buffer[i] == '$') {
            buffer[i] = '\0';
        }
    }

    char *status = buffer;
    char *message = "";
    for (int i = 0; i < len; i++) {
        if (buffer[i] == '\0') {
            message = &buffer[i + 1];
            break;
        }
    }

    printf("\n********************************\n  %s\n********************************\n", message);
    return strcmp(status, "OK") == 0;
}

void do_register(int cfd) {
    char username[256], password[256], buffer[1024];
    get_input("Enter username: ", username, sizeof(username));
    get_input("Enter password: ", password, sizeof(password));

    int len = snprintf(buffer, sizeof(buffer), "REG$%s$%s", username, password);
    send(cfd, buffer, len, 0);
    handle_server_response(cfd);
}

void do_login(int cfd) {
    char username[256], password[256], buffer[1024];
    get_input("Enter username: ", username, sizeof(username));
    get_input("Enter password: ", password, sizeof(password));

    int len = snprintf(buffer, sizeof(buffer), "LOGIN$%s$%s", username, password);
    send(cfd, buffer, len, 0);

    if (handle_server_response(cfd)) {
        g_logged_in = 1;
        strcpy(g_username, username);
    }
}

void do_logout() {
    g_logged_in = 0;
    memset(g_username, 0, sizeof(g_username));
    printf("You have been logged out.\n");
}

void do_change_password(int cfd) {
    char old_pass[256], new_pass[256], buffer[1024];
    get_input("Enter old password: ", old_pass, sizeof(old_pass));
    get_input("Enter new password: ", new_pass, sizeof(new_pass));

    int len = snprintf(buffer, sizeof(buffer), "CHGPWD$%s$%s", old_pass, new_pass);
    send(cfd, buffer, len, 0);
    handle_server_response(cfd);
}

void do_add_friend(int cfd) {
    char friend_name[256], buffer[1024];
    get_input("Enter friend's username to add: ", friend_name, sizeof(friend_name));

    int len = snprintf(buffer, sizeof(buffer), "ADDFRIEND$%s", friend_name);
    send(cfd, buffer, len, 0);
    handle_server_response(cfd);
}

void do_del_friend(int cfd) {
    char friend_name[256], buffer[1024];
    get_input("Enter friend's username to delete: ", friend_name, sizeof(friend_name));

    int len = snprintf(buffer, sizeof(buffer), "DELFRIEND$%s", friend_name);
    send(cfd, buffer, len, 0);
    handle_server_response(cfd);
}

void do_chat(int cfd) {
    char recipient[256];
    get_input("Enter username to chat with: ", recipient, sizeof(recipient));
    printf("--- Entering chat with %s. Type 'Q' on a new line to exit. ---\n", recipient);

    fd_set read_fds;
    char send_buf[1024];
    char recv_buf[1024];

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(cfd, &read_fds);

        int max_fd = (STDIN_FILENO > cfd) ? STDIN_FILENO : cfd;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("select error\n");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            fgets(send_buf, sizeof(send_buf), stdin);
            if (strcmp(send_buf, "Q\n") == 0 || strcmp(send_buf, "q\n") == 0) {
                break;
            }
            send_buf[strcspn(send_buf, "\n")] = 0;
            char packet[1024];
            int len = snprintf(packet, sizeof(packet), "MSG$%s$%s", recipient, send_buf);
            send(cfd, packet, len, 0);
        }

        if (FD_ISSET(cfd, &read_fds)) {
            int len = recv(cfd, recv_buf, sizeof(recv_buf) - 1, 0);
            if (len <= 0) {
                printf("Server disconnected.\n");
                g_logged_in = 0;
                break;
            }
            recv_buf[len] = '\0';

            // Replace '$' with '\0'
            for (int i = 0; i < len; i++) {
                if (recv_buf[i] == '$') {
                    recv_buf[i] = '\0';
                }
            }

            char *cmd = recv_buf;
            char *sender = "";
            char *message = "";
            int part = 0;
            for (int i = 0; i < len; ++i) {
                if (recv_buf[i] == '\0') {
                    part++;
                    if (part == 1) sender = &recv_buf[i + 1];
                    if (part == 2) message = &recv_buf[i + 1];
                }
            }

            if (strcmp(cmd, "MSG") == 0) {
                printf("[%s]: %s\n", sender, message);
            } else {
                printf("[Server]: %s\n", (strchr(recv_buf, '\0') + 1));
            }
        }
    }
    printf("--- Exited chat with %s. ---\n", recipient);
}

void show_menu() {
    printf("\n----------- MENU -----------\n");
    if (!g_logged_in) {
        printf("1. Register\n");
        printf("2. Login\n");
    } else {
        printf("Logged in as: %s\n", g_username);
        printf("3. Change Password\n");
        printf("4. Add Friend\n");
        printf("5. Delete Friend\n");
        printf("6. Chat\n");
        printf("7. Logout\n");
    }
    printf("0. Exit\n");
    printf("--------------------------\n");
    printf("Enter choice: ");
}

int main() {
    int cfd;
    struct sockaddr_in s_add;
    unsigned short portnum = 2333;

    cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == cfd) {
        perror("socket fail");
        return -1;
    }

    bzero(&s_add, sizeof(struct sockaddr_in));
    s_add.sin_family = AF_INET;
    s_add.sin_addr.s_addr = inet_addr("127.0.0.1");  // 恢复为原来的IP地址
    s_add.sin_port = htons(portnum);

    if (-1 == connect(cfd, (struct sockaddr *)(&s_add), sizeof(struct sockaddr))) {
        perror("connect fail");
        close(cfd);
        return -1;
    }

    printf("Connected to server!\n");

    int choice = -1;
    while (choice != 0) {
        show_menu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n');  // Clear input buffer
            choice = -1;
            continue;
        }
        while (getchar() != '\n');  // Clear trailing newline

        if (g_logged_in) {
            switch (choice) {
                case 3:
                    do_change_password(cfd);
                    break;
                case 4:
                    do_add_friend(cfd);
                    break;
                case 5:
                    do_del_friend(cfd);
                    break;
                case 6:
                    do_chat(cfd);
                    break;
                case 7:
                    do_logout();
                    break;
                case 0:
                    break;
                default:
                    printf("Invalid choice. Try again.\n");
                    break;
            }
        } else {
            switch (choice) {
                case 1:
                    do_register(cfd);
                    break;
                case 2:
                    do_login(cfd);
                    break;
                case 0:
                    break;
                default:
                    printf("Invalid choice. Please log in first.\n");
                    break;
            }
        }
    }

    close(cfd);
    return 0;
}
