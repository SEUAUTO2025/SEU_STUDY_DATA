#include <stdio.h>      // 标准输入输出函数库
#include <stdlib.h>     // 标准库函数，比如 atoi()
#include <fcntl.h>      // 文件控制函数库，提供 open() 等函数
#include <errno.h>      // 错误号定义
#include <unistd.h>     // 提供 read(), write(), usleep() 等函数
#include <sys/types.h>  // 系统数据类型定义
#include <sys/stat.h>   // 文件状态相关函数
#include <string.h>     // 字符串处理函数
#include <stdint.h>     // 定义标准整数类型（如 int32_t）
#include <termios.h>    // 终端 I/O 设置
#include <math.h>       // 数学库函数，比如 log()

// 从 ADC 设备读取数值，并转换为阻值
int read_adc_value(int fd_adc, char *buffer)
{
    int len = read(fd_adc, buffer, 10); // 从 /dev/adc 读取最多 10 字节存入 buffer
    if (len == 0) // 如果读取失败或没数据
    {
        printf("ADC read error \n");
        return;   // 没有返回具体值（有点不规范，最好 return -1）
    }
    return atoi(buffer) * 10000 / 4095; // 将 ADC 数字量转为阻值（按 10kΩ 电阻换算）
}

// 控制 LED 闪烁
void led_blink(int fd_led, float duty, float period)//duty应该是灭灯占的比例
{
    // 点亮 LED
    ioctl(fd_led, 0, 0); // LED 设备控制（参数含义根据驱动定义）
    ioctl(fd_led, 0, 1);
    // 保持亮的时间 = 周期 * 占空比
    usleep((int)(1000000 * period * duty));

    // 打印调试信息
    printf("Period=%f,duty=%f\n", period , duty);
    printf("T value: %d\n", (int)(1000000 *period * duty));

    // 熄灭 LED
    ioctl(fd_led, 1, 0);//第三个参数是灯号，1是灭，0是亮
    ioctl(fd_led, 1, 1);
    // 保持灭的时间 = 周期 * (1-占空比)
    usleep((int)(1000000 * period * (1 - duty)));
}

int main()
{
    int choice;   // 用户输入选择
    int fd_adc;   // ADC 文件描述符
    int fd_led;   // LED 文件描述符
    int fd_bz;    // 蜂鸣器文件描述符
    char *adc = "/dev/adc";            // ADC 设备文件路径
    char *leds = "/dev/leds";          // LED 设备文件路径
    char *buzzer = "/dev/buzzer_ctl";  // 蜂鸣器设备文件路径
    char buffer[16];  // 存放 ADC 读取结果的缓冲区
    int r = 0;        // 阻值变量

    // 打开 ADC 设备
    if ((fd_adc = open(adc, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)//可读可写；如果打开的文件是终端设备，不要设为当前进程的控制终端；非阻塞式
    {
        printf("open ADC error\n");
        return;
    }

    // 打开 LED 设备
    if ((fd_led = open(leds, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        printf("open LED error\n");
        return;
    }

    // 打开蜂鸣器设备
    if ((fd_bz = open(buzzer, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) 
    {
        printf("open buzzer error\n");
        return;
    }

    // 初始化 buffer
    memset(buffer, 0, sizeof(buffer));

    // 主循环：菜单选择
    while (1)
    {
        printf("Choose a task (0-5, 10 to exit): ");
        scanf("%d", &choice);

        switch (choice)
        {
        case 0:
            // 任务 0: 打印 Hello World
            printf("Hello, World!\n");
            break;

        case 1:
            // 任务 1: 循环读取 ADC 值并打印
            while (1)
            {
                printf("R value: %d\n", read_adc_value(fd_adc, buffer));
                usleep(1000000); // 每 1 秒读取一次
            }
            break;

        case 2:
            // 任务 2: LED 按 0.5 占空比，1 秒周期闪烁
            while (1)
            {
                led_blink(fd_led, 0.5, 1);
            }
            break;

        case 3:
            // 任务 3: LED 闪烁周期随阻值变化
            while(1)
            {
                r = read_adc_value(fd_adc, buffer);
                printf("R value: %d\n", r);
                // 周期 = 1000.0 / r 秒，阻值越大，周期越短
                led_blink(fd_led, 0.5, 1 * 1000.0/r); 
            }

        case 4:
            // 任务 4: 阻值过小(<1000) 或过大(>9000) 时蜂鸣器报警
            while(1)
            {
                r = read_adc_value(fd_adc, buffer);
                printf("R value: %d\n", r);
                if (r < 1000 || r > 9000)
                {
                    // 亮灯并蜂鸣器响
                    ioctl(fd_led, 1, 0);
                    ioctl(fd_led, 1, 1);
                    ioctl(fd_bz, 1);   // 打开蜂鸣器
                    usleep(500000);
                    ioctl(fd_bz, 0);   // 关闭蜂鸣器
                    usleep(500000);
                }
                else
                {
                    // 正常模式：按阻值控制 LED 闪烁
                    led_blink(fd_led, 0.5, 1 * 1000.0/r);
                }
            }

        case 5:
            // 任务 5: 类似 case 4，但闪烁频率用对数函数控制
            while(1){
                r = read_adc_value(fd_adc, buffer);
                printf("R value: %d\n", r);
                if (r < 1000 || r > 9000)
                {
                    // 阻值异常 -> 报警
                    ioctl(fd_led, 1, 0);
                    ioctl(fd_led, 1, 1);
                    ioctl(fd_bz, 1);
                    usleep(500000);
                    ioctl(fd_bz, 0);
                    usleep(500000);
                }
                else
                {
                    // 周期 = 0.25 / (0.125 + log(r/1000))
                    // 频率 = (0.125 + ln(r/1000)) * 4，范围大约 0.5Hz - 8.5Hz
                    led_blink(fd_led, 0.5, 0.25 / (0.125 + log(r / 1000))); 
                }
            }
            break;

        case 10:
            // 退出程序
            return 0;

        default:
            // 输入非法选项
            printf("Invalid choice, try again.\n");
        }
    }
    return 0;
}
