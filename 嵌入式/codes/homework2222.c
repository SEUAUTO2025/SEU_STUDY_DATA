#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
int set_opt(int fd_uart, int nSpeed, int nBits, char nEvent, int nStop);
long long get_timestamp(void);               // 获取时间戳函数
void get_format_time_string(char *str_time); // 获取格式化时间
int read_adc_value(int fd_adc, char *buffer)
{
    int len = read(fd_adc, buffer, 12);
    if (len == 0)
    {
        printf("ADC read error \n");
        return;
    }
    return atoi(buffer) * 10000 / 4095;
}

void led_blink(int fd_led, float duty, float period)
{
    ioctl(fd_led, 0, 0);
    ioctl(fd_led, 0, 1);
    usleep((int)(1000000 * period * duty));
    // printf("Period=%f,duty=%f\n", period , duty);
    printf("T value: %f.2\n", period);

    ioctl(fd_led, 1, 0);
    ioctl(fd_led, 1, 1);
    usleep((int)(1000000 * period * (1 - duty)));
}

int main() {
    int choice;                          // 用户菜单选择
    int fd_adc;                          // ADC文件描述符
    int fd_led;                          // LED文件描述符
    int fd_bz;                           // 蜂鸣器文件描述符
    int fd_uart, fd_file1, fd_file2;     // 串口、文件1、文件2的文件描述符
    char *adc = "/dev/adc";              // ADC设备路径
    char *leds = "/dev/leds";            // LED设备路径
    char *buzzer = "/dev/buzzer_ctl";    // 蜂鸣器设备路径
    char *uart1 = "/dev/ttySAC3";        // 串口设备路径

    char uart_out[1000] = "";            // 串口输出缓冲区
    char buffer[16];                     // ADC数据缓冲
    char format_time_string[100];        // 格式化时间字符串缓冲
    int r = 0;                           // 记录ADC转换结果（电阻值）
    long long last_time = 0;             // 记录上一次写文件的时间戳

    // 初始化内存，避免脏数据
    memset(buffer, 0, sizeof(buffer));   
    memset(format_time_string, 0, sizeof(format_time_string)); // 这里写1000会越界，应改为 sizeof(format_time_string)
    memset(uart_out, 0, sizeof(uart_out));            // 这里写500也会越界，应改为 sizeof(uart_out)

    // 打开ADC设备
    if ((fd_adc = open(adc, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        printf("open ADC error\n");
        return;
    }
    // 打开LED设备
    if ((fd_led = open(leds, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        printf("open LED error\n");
        return;
    }
    // 打开蜂鸣器设备
    if ((fd_bz = open(buzzer, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        printf("open buzzer error\n");
        return;
    }
    // 打开串口设备
    if ((fd_uart = open(uart1, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        printf("open uart1 error\n");
        return;
    } else {
        set_opt(fd_uart, 9600, 8, 'N', 1); // 设置串口：9600bps，8位数据，无校验，1位停止位
    }

    // 主循环，用户通过输入数字选择任务
    while (1) {
        printf("Choose a task (0-4, 10 to exit): ");
        scanf("%d", &choice); // 输入选择

        switch (choice) {
        case 0: // 简单打印
            printf("Hello, World!\n");
            break;

        case 1: // 不断发送 "hello" 到串口
            while (1) {
                sprintf(uart_out, "hello\r\n");           // 格式化写入缓冲
                write(fd_uart, uart_out, strlen(uart_out)); // 发送到串口
                sleep(1); // 每秒一次
            }
            break;

        case 2: // 读取ADC并根据值控制蜂鸣器和LED
            while (1) {
                r = read_adc_value(fd_adc, buffer); // 获取ADC值
                if (r < 1000) { // 太低报警
                    sprintf(uart_out, "Resistnce:%d Ohm ,Alert:Too low!\r\n", r);
                    write(fd_uart, uart_out, strlen(uart_out)); // 发送到串口
                    ioctl(fd_led, 1, 0); // 点亮LED
                    ioctl(fd_led, 1, 1);
                    ioctl(fd_bz, 1);    // 打开蜂鸣器
                    usleep(500000);     // 500ms
                    ioctl(fd_bz, 0);    // 关闭蜂鸣器
                    usleep(500000);
                } else if (r > 9000) { // 太高报警
                    sprintf(uart_out, "Resistnce:%d Ohm ,Alert:Too high!\r\n", r);
                    write(fd_uart, uart_out, strlen(uart_out));
                    ioctl(fd_led, 1, 0);
                    ioctl(fd_led, 1, 1);
                    ioctl(fd_bz, 1);
                    usleep(500000);
                    ioctl(fd_bz, 0);
                    usleep(500000);
                } else { // 正常范围
                    sprintf(uart_out, "Resistnce:%d Ohm ,Alert:None!\r\n", r);
                    write(fd_uart, uart_out, strlen(uart_out));
                    // LED闪烁，频率依赖电阻值
                    led_blink(fd_led, 0.5, 4 / (0.125 + pow(1.7, r / 1000.0))); 
                }
            }
            break;

        case 3: // 记录格式化时间到文件，并回读打印
            get_format_time_string(format_time_string); // 获取时间
            sprintf(uart_out, "08022107 syw etc\r\n format time:%s\r\n", format_time_string);

            // 写文件 /home/code/2_3.txt
            if ((fd_file1 = open("/home/code/2_3.txt", O_RDWR | O_CREAT | O_APPEND, 0777)) < 0) {
                printf("open /home/code/2_3.txt failed!\r\n");
                return;
            }
            write(fd_file1, uart_out, strlen(uart_out)); // 写入
            close(fd_file1);

            // 重新打开并读取文件
            if ((fd_file1 = open("/home/code/2_3.txt", O_RDWR | O_CREAT | O_APPEND, 0777)) < 0) {
                printf("open /home/code/2_3.txt failed!\r\n");
                return;
            }
            read(fd_file1, uart_out, 1000); // 读回
            close(fd_file1);

            printf("%s", uart_out); // 打印
            write(fd_uart, uart_out, strlen(uart_out)); // 发送到串口
            break;

        case 4: // 实时采集+存文件+加时间戳
            while (1) {
                (long long)get_timestamp(); // 获取当前时间戳（没用上）

                r = read_adc_value(fd_adc, buffer); // 读取ADC
                printf("R value: %d\n", r);

                if (r < 1000 || r > 9000) { // 报警
                    sprintf(uart_out, "Resistnce:%d ,Alert\r\n", r);
                    ioctl(fd_led, 1, 0);
                    ioctl(fd_led, 1, 1);
                    ioctl(fd_bz, 1);
                    usleep(500000);
                    ioctl(fd_bz, 0);
                    usleep(500000);
                } else { // 正常
                    get_format_time_string(format_time_string); // 获取时间
                    sprintf(uart_out, "Resistnce:%d ,OK,%s\r\n", r, format_time_string);
                    // LED随电阻值闪烁
                    led_blink(fd_led, 0.5, 4 / (0.125 + pow(1.7, r / 1000.0)));
                }

                // 每1秒写一次文件并回传
                if (get_timestamp() - last_time >= 1000) {
                    if ((fd_file2 = open("/home/code/2_4.txt", O_RDWR | O_CREAT | O_APPEND, 0777)) < 0) {
                        printf("open /home/code/2_4.txt failed!\r\n");
                        return;
                    }
                    write(fd_file2, uart_out, strlen(uart_out));
                    close(fd_file2);

                    if ((fd_file2 = open("/home/code/2_4.txt", O_RDWR | O_CREAT | O_APPEND, 0777)) < 0) {
                        printf("open /home/code/2_4.txt failed!\r\n");
                        return;
                    }
                    read(fd_file2, uart_out, 1000);
                    close(fd_file2);

                    write(fd_uart, uart_out, strlen(uart_out)); // 通过串口回传
                    last_time = get_timestamp(); // 更新时间戳
                }
            }
            break;

        case 10: // 退出程序
            return 0;

        default: // 输入错误
            printf("Invalid choice, try again.\n");
            break;
        }
    }
    return 0;
}


long long get_timestamp(void) // 获取时间戳函数
{
    long long tmp;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    tmp = tv.tv_sec;
    tmp = tmp * 1000;
    tmp = tmp + (tv.tv_usec / 1000);

    return tmp;
}
void get_format_time_string(char *str_time) // 获取格式化时间
{
    time_t now;
    struct tm *tm_now;
    char datetime[128];

    time(&now);
    tm_now = localtime(&now);//内置函数
    strftime(datetime, 128, "%Y-%m-%d %H:%M:%S", tm_now);//把获取到的时间戳转成字符串

    printf("now datetime : %s\n", datetime);
    strcpy(str_time, datetime);
}