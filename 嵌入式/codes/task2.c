#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int set_opt(int fd_uart, int nSpeed, int nBits, char nEvent, int nStop);
int read_adc_value(int fd_adc, char *buffer);
long long get_timestamp(void);               // 获取时间戳函数
void get_format_time_string(char *str_time); // 获取格式化时间

int main() {
    int fd_adc, fd_led, fd_bz, fd_uart;
    char *adc = "/dev/adc";
    char *leds = "/dev/leds";
    char *buzzer = "/dev/buzzer_ctl";
    char *uart1 = "/dev/ttySAC3";
    if ((fd_adc = open(adc, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        printf("open ADC error\n");
        return 1;
    }
    if ((fd_led = open(leds, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        printf("open LED error\n");
        return 1;
    }
    if ((fd_bz = open(buzzer, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        printf("open buzzer error\n");
        return 1;
    }
    if ((fd_uart = open(uart1, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        printf("open uart1 error\n");
        return 1;
    } else {
        set_opt(fd_uart, 9600, 8, 'N', 1);
    }


    // Initialize clock for 100Hz loop
    clock_t last_clock_time = 0;

    //////////////////////////////////////////////////////////////////////////
    // new
    char uart_out[1000] = "";
    char buffer[16];
    char format_time_string[100];
    int fd_file2;
    long long start_time = get_timestamp();
    long long current_time;
    int duration = 15000; // 15秒，以毫秒为单位
    //////////////////////////////////////////////////////////////////////////

    int thresh_high = 9000;
    int thresh_low = 5000;
    float flash_freq = 5.0;
    int led_period = 20;

    int loop_times = 0;
    int special_phase = 0;
    bool led_on = false;
    int r = 0;

    ioctl(fd_led, 0, 0);
    ioctl(fd_led, 0, 1);
    led_on = false;

    int mode = 0;

    char adc_tmp[16];
    char uart_rx_tmp[100];
    char uart_rx_buf[100];
    int uart_rx_cnt = 0;

    while (1) {
        // Make sure loop runs at 100Hz
        if (last_clock_time != 0 && (double)(clock() - last_clock_time) / CLOCKS_PER_SEC < 0.01) {
            usleep(100);
            continue;
        }
        last_clock_time = clock();

        loop_times++;

        if (loop_times % 10 == 0) {
            r = read_adc_value(fd_adc, adc_tmp);

            int bytes_read = read(fd_uart, uart_rx_tmp, 100);
            if (bytes_read > 0) {
                memcpy(uart_rx_buf + uart_rx_cnt, uart_rx_tmp, bytes_read);
                uart_rx_cnt += bytes_read;
                if (uart_rx_buf[uart_rx_cnt - 1] == 0x7D) {
                    // Print as hex
                    printf("\n\nReceived message: ");
                    for (int i = 0; i < uart_rx_cnt; i++) {
                        printf("%02X ", uart_rx_buf[i]);
                    }
                    printf("\n");

                    mode = uart_rx_buf[1];

                    if (mode == 1) {
                        thresh_low = uart_rx_buf[2] << 8 | uart_rx_buf[3];
                        thresh_high = uart_rx_buf[4] << 8 | uart_rx_buf[5];
                        flash_freq = uart_rx_buf[6] << 8 | uart_rx_buf[7];
                        if (flash_freq > 0) {
                            led_period = (int)(100.0 / flash_freq);
                            if (led_period < 2) led_period = 2;
                        }
                        mode = 0;
                    } else if (mode == 2) {
                        char uart_tx_buf[] = {0x7B, 0x27, 0x10, 0x01, 0x02, 0x03, 0x4E, 0x7D};
                        uart_tx_buf[1] = r >> 8;
                        uart_tx_buf[2] = r & 0xFF;
                        if (special_phase == 0) {
                            uart_tx_buf[3] = 'o';
                            uart_tx_buf[4] = 'k';
                            uart_tx_buf[5] = ' ';
                        } else if (special_phase == 1) {
                            uart_tx_buf[3] = 'h';
                            uart_tx_buf[4] = 'i';
                            uart_tx_buf[5] = 'g';
                        } else if (special_phase == 2) {
                            uart_tx_buf[3] = 'l';
                            uart_tx_buf[4] = 'o';
                            uart_tx_buf[5] = 'w';
                        }

                        // Calculate BCC checksum to uart_tx_buf[6]
                        uart_tx_buf[6] = 0;
                        for (int i = 0; i < 6; i++) {
                            uart_tx_buf[6] ^= uart_tx_buf[i];
                        }

                        // Print sent message as hex
                        printf("Sent message: ");
                        for (int i = 0; i < 8; i++) {
                            printf("%02X ", uart_tx_buf[i]);
                        }
                        printf("\n");

                        write(fd_uart, uart_tx_buf, 8);
                        mode = 0;
                    } else if (mode == 3) {
                        char uart_tx_buf[] = {0x7B, 0x27, 0x10, 0x01, 0x02, 0x03, 0x4E, 0x7D};
                        uart_tx_buf[1] = r >> 8;
                        uart_tx_buf[2] = r & 0xFF;
                        if (special_phase == 0) {
                            uart_tx_buf[3] = 'o';
                            uart_tx_buf[4] = 'k';
                            uart_tx_buf[5] = ' ';
                        } else if (special_phase == 1) {
                            uart_tx_buf[3] = 'h';
                            uart_tx_buf[4] = 'i';
                            uart_tx_buf[5] = 'g';
                        } else if (special_phase == 2) {
                            uart_tx_buf[3] = 'l';
                            uart_tx_buf[4] = 'o';
                            uart_tx_buf[5] = 'w';
                        }

                        // Calculate BCC checksum to uart_tx_buf[6]
                        uart_tx_buf[6] = 0;
                        for (int i = 0; i < 6; i++) {
                            uart_tx_buf[6] ^= uart_tx_buf[i];
                        }

                        // Print sent message as hex
                        printf("Original message: ");
                        for (int i = 0; i < 8; i++) {
                            printf("%02X ", uart_tx_buf[i]);
                        }
                        printf("\n");

                        // XOR Encrypt
                        for (int i = 1; i < 7; i++) {
                            uart_tx_buf[i] ^= 0xAA;
                        }

                        // Print sent message as hex
                        printf("Sent Encrypted message: ");
                        for (int i = 0; i < 8; i++) {
                            printf("%02X ", uart_tx_buf[i]);
                        }
                        printf("\n");

                        write(fd_uart, uart_tx_buf, 8);
                        mode = 0;
                    } else if (mode == 4) {
                        start_time = get_timestamp();
                        // 打开文件，使用写入模式（覆盖）
                        if ((fd_file2 = open("2_4.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777)) < 0) {
                            printf("open 2_4.txt failed!\r\n");
                            break;
                        }

                        // 循环15秒
                        do {
                            current_time = get_timestamp();
                            // 读取ADC
                            r = read_adc_value(fd_adc, buffer);
                            // 获取格式化时间
                            get_format_time_string(format_time_string);
                            // 判断报警
                            if (r < 1000 || r > 9000) {
                                sprintf(uart_out, "Resistnce:%d ,Alert,%s\r\n", r, format_time_string);
                            } else {
                                sprintf(uart_out, "Resistnce:%d ,OK,%s\r\n", r, format_time_string);
                            }
                            // 写入文件
                            write(fd_file2, uart_out, strlen(uart_out));

                            // 等待100ms
                            usleep(100000);
                        } while (current_time - start_time < duration);

                        close(fd_file2);

                        // 现在重新打开文件读取内容
                        if ((fd_file2 = open("2_4.txt", O_RDONLY)) < 0) {
                            printf("open 2_4.txt for read failed!\r\n");
                            break;
                        }

                        // 读取文件内容并发送
                        printf("File content: ");
                        int read_len;
                        while ((read_len = read(fd_file2, uart_out, sizeof(uart_out))) > 0) {
                            write(fd_uart, uart_out, read_len);
                            // Print sent file content as text
                            for (int i = 0; i < read_len; i++) {
                                printf("%c", uart_out[i]);
                            }
                        }
                        // 发送结束标志
                        char end[2] = {0xff, 0xff};
                        write(fd_uart, end, 2);
                        close(fd_file2);
                        printf("\n");

                        // 提示完成
                        printf("File content SENT!\n");
                        mode = 0;
                    }

                    uart_rx_cnt = 0;
                }
            }
        }

        if (special_phase == 0) {
            if (r > thresh_high) {
                special_phase = 1;
                loop_times = 0;
                ioctl(fd_bz, 1);
            } else if (r < thresh_low) {
                special_phase = 2;
                loop_times = 0;
                ioctl(fd_bz, 1);
            }
        } else {
            bool new_led_on = (loop_times % led_period) < (led_period / 2);
            if (new_led_on != led_on) {
                led_on = new_led_on;
                if (led_on) {
                    ioctl(fd_led, 1, 0);
                    ioctl(fd_led, 1, 1);
                } else {
                    ioctl(fd_led, 0, 0);
                    ioctl(fd_led, 0, 1);
                }
            }

            if (r > thresh_low && r < thresh_high) {
                special_phase = 0;
                ioctl(fd_bz, 0);
                led_on = false;
                ioctl(fd_led, 0, 0);
                ioctl(fd_led, 0, 1);
            }
        }

        usleep(1000);
    }
}

int read_adc_value(int fd_adc, char *buffer) {
    int len = read(fd_adc, buffer, 12);
    if (len == 0) {
        printf("ADC read error \n");
        return 0;
    }
    return atoi(buffer) * 10000 / 4095;
}

int set_opt(int fd_uart, int nSpeed, int nBits, char nEvent, int nStop) {
    struct termios newtio, oldtio;           // 定义新旧两个termios结构体
    if (tcgetattr(fd_uart, &oldtio) != 0) {  // tcgetattr读取当期串口参数，确认串口是否可以配置（返回0时为执行成功）
        perror("SetupSerial 1");
        return -1;
    }
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    switch (nBits)  // 设置数据位
    {
        case 7:
            newtio.c_cflag |= CS7;
            break;
        case 8:
            newtio.c_cflag |= CS8;
            break;
    }

    switch (nEvent)  // 设置奇偶校验
    {
        case 'O':
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            newtio.c_iflag |= (INPCK | ISTRIP);
            break;
        case 'E':
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;
        case 'N':
            newtio.c_cflag &= ~PARENB;
            break;
    }

    switch (nSpeed)  // 设置波特率
    {
        case 2400:
            cfsetispeed(&newtio, B2400);
            cfsetospeed(&newtio, B2400);
            break;
        case 4800:
            cfsetispeed(&newtio, B4800);
            cfsetospeed(&newtio, B4800);
            break;
        case 9600:
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
        case 115200:
            cfsetispeed(&newtio, B115200);
            cfsetospeed(&newtio, B115200);
            break;
        case 460800:
            cfsetispeed(&newtio, B460800);
            cfsetospeed(&newtio, B460800);
            break;
        default:
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
    }
    if (nStop == 1)
        newtio.c_cflag &= ~CSTOPB;
    else if (nStop == 2)
        newtio.c_cflag |= CSTOPB;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd_uart, TCIFLUSH);                       // 清除寄存器
    if ((tcsetattr(fd_uart, TCSANOW, &newtio)) != 0)  // 设置新参数（均存于结构体newtio中）
    {
        perror("com set error");
        return -1;
    }

    //	printf("set done!\n\r");
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
    tm_now = localtime(&now);
    strftime(datetime, 128, "%Y-%m-%d %H:%M:%S", tm_now);

    printf("now datetime : %s\n", datetime);
    strcpy(str_time, datetime);
}

