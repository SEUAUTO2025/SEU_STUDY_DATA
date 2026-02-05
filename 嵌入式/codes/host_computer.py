#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import serial  # pyright: ignore[reportMissingModuleSource]
import time
import os
import struct

class HostComputer:
    def __init__(self, port='/dev/ttyUSB0', baudrate=9600):
        self.port = port
        self.baudrate = baudrate
        self.serial_conn = None

    def connect_serial(self):
        """连接串口"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,#八位
                parity=serial.PARITY_NONE,#无校验
                stopbits=serial.STOPBITS_ONE,#一位停止位
                timeout=1#超时时间
            )
            print(f"串口连接成功: {self.port} @ {self.baudrate}bps")
            return True
        except serial.SerialException as e:
            print(f"串口连接失败: {e}")
            print("请检查：")
            print("1. 开发板是否已连接到电脑")
            print("2. 串口设备名称是否正确")
            print("3. 串口是否被其他程序占用")
            return False

    def list_serial_ports(self):
        """列出可用的串口设备"""
        import glob
        ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
        if ports:
            print("可用的串口设备:")
            for port in ports:
                print(f"  {port}")
        else:
            print("未找到可用的串口设备")
            print("请确保开发板已连接到电脑")
        return ports

    def disconnect_serial(self):
        """断开串口连接"""
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            print("串口连接已断开")

    def calculate_bcc(self, data):
        """计算BCC校验码"""
        bcc = 0
        for byte in data:
            bcc ^= byte
        return bcc

    def parse_data_frame(self, frame):
        """解析数据帧"""
        if len(frame) != 8:
            return None

        # 检查帧头和帧尾
        if frame[0] != 0x7B or frame[7] != 0x7D:
            return None

        # 提取阻值 (第2-3字节)
        resistance = (frame[1] << 8) | frame[2]

        # 提取报警状态 (第4-6字节)
        alarm_bytes = frame[3:6]
        try:
            alarm_status = ''.join(chr(b) for b in alarm_bytes)
        except:
            alarm_status = "ERR"

        # 验证BCC校验 (第7字节)
        calculated_bcc = self.calculate_bcc(frame[:6])
        received_bcc = frame[6]

        return {
            'resistance': resistance,
            'alarm_status': alarm_status,
            'calculated_bcc': calculated_bcc,
            'received_bcc': received_bcc,
            'bcc_valid': calculated_bcc == received_bcc
        }

    def xor_encrypt_decrypt(self, data, key=0xAA):
        """XOR加密/解密"""
        return bytes(b ^ key for b in data)

    def send_command(self, command):
        """发送命令到板子"""
        if not self.serial_conn or not self.serial_conn.is_open:
            print("串口未连接")
            return False

        try:
            # 确保command是bytes类型
            if isinstance(command, str):
                command = command.encode('utf-8')

            # 构造完整的命令帧：帧头 + 原始命令 + BCC校验 + 帧尾
            frame_header = b'\x7B'  # 帧头 0x7B
            frame_tail = b'\x7D'    # 帧尾 0x7D


            # 构造完整帧
            complete_frame = frame_header + command + frame_tail

            self.serial_conn.write(complete_frame)
            print(f"发送命令帧: {complete_frame.hex().upper()}")
            print(f"原始命令: {command.hex().upper()}")
            return True
        except Exception as e:
            print(f"发送命令失败: {e}")
            return False

    def receive_data_frame(self):
        """接收数据帧"""
        if not self.serial_conn or not self.serial_conn.is_open:
            print("串口未连接")
            return None

        try:
            # 读取完整的数据帧 (8字节)
            frame = self.serial_conn.read(8)
            if len(frame) == 8:
                return frame
            else:
                return None
        except Exception as e:
            print(f"接收数据失败: {e}")
            return None

    def case1_parameter_adjustment(self):
        """Case 1: 参数调节"""
        print("\n=== Case 1: 参数调节 ===")

        # 获取用户输入的参数
        try:
            lower_threshold = int(input("请输入电阻报警下阈值: "))
            upper_threshold = int(input("请输入电阻报警上阈值: "))
            led_frequency = int(input("请输入LED闪烁频率(Hz): "))

            # 构造参数调节命令
            # 原始格式: CMD(1字节) + 下阈值(2字节) + 上阈值(2字节) + 频率(2字节)
            # 完整帧格式: 帧头(1) + 原始命令(7) + BCC(1) + 帧尾(1) = 10字节
            command = struct.pack('>BHHH',
                                0x01,  # CMD = 1 for parameter adjustment
                                lower_threshold,
                                upper_threshold,
                                led_frequency)

            if self.send_command(command):
                print("参数调节命令已发送")
            else:
                print("参数调节失败")

        except ValueError:
            print("输入无效，请输入有效的数字")

    def case2_data_receive(self):
        """Case 2: 接收并解析数据帧"""
        print("\n=== Case 2: 数据接收与解析 ===")

        # 发送进入Case 2的命令
        # 原始命令: CMD = 2 (1字节)
        # 完整帧格式: 帧头(1) + 原始命令(1) + BCC(1) + 帧尾(1) = 4字节
        self.send_command(b'\x02')  # CMD = 2

        while True:
            frame = self.receive_data_frame()
            if frame:
                print(f"\n收到数据帧: {frame.hex().upper()}")

                parsed = self.parse_data_frame(frame)
                if parsed:
                    print(f"阻值: {parsed['resistance']}")
                    print(f"报警状态: {parsed['alarm_status']}")
                    print(f"BCC校验: {'有效' if parsed['bcc_valid'] else '无效'}")
                    print(f"计算得BCC: 0x{parsed['calculated_bcc']:02X}, 接收到BCC: 0x{parsed['received_bcc']:02X}")
                else:
                    print("数据帧格式错误")

                # 询问是否继续接收
                choice = input("\n按Enter键继续接收，输入'q'返回主菜单: ")
                if choice.lower() == 'q':
                    break
            else:
                print("未收到数据帧")
                time.sleep(0.1)

    def case3_encrypted_receive(self):
        """Case 3: 接收加密数据并解密"""
        print("\n=== Case 3: 加密数据接收与解密 ===")

        # 发送进入Case 3的命令
        # 原始命令: CMD = 3 (1字节)
        # 完整帧格式: 帧头(1) + 原始命令(1) + BCC(1) + 帧尾(1) = 4字节
        self.send_command(b'\x03')  # CMD = 3

        while True:
            frame = self.receive_data_frame()
            if frame:
                print(f"\n收到加密数据帧: {frame.hex().upper()}")

                # 对整个帧进行解密
                decrypted_frame = self.xor_encrypt_decrypt(frame)
                print(f"直接解密后数据帧: {decrypted_frame.hex().upper()}")

                # 修正帧头和帧尾（解密过程中帧头帧尾也被加密了，需要改回来）
                if len(decrypted_frame) >= 8:  # 确保是完整的数据帧
                    corrected_frame = b'\x7B' + decrypted_frame[1:-1] + b'\x7D'
                    print(f"修正帧头帧尾后: {corrected_frame.hex().upper()}")

                    # 解析修正后的数据
                    parsed = self.parse_data_frame(corrected_frame)
                    if parsed:
                        print(f"阻值: {parsed['resistance']}")
                        print(f"报警状态: {parsed['alarm_status']}")
                        print(f"BCC校验: {'有效' if parsed['bcc_valid'] else '无效'}")
                        print(f"计算得BCC: 0x{parsed['calculated_bcc']:02X}, 接收到BCC: 0x{parsed['received_bcc']:02X}")
                    else:
                        print("解密后数据帧格式错误")
                else:
                    print("接收到的帧长度不正确")

                # 询问是否继续接收
                choice = input("\n按Enter键继续接收，输入'q'返回主菜单: ")
                if choice.lower() == 'q':
                    break
            else:
                print("未收到数据帧")
                time.sleep(0.1)

    def case4_file_transfer(self):
        """Case 4: 文件传输"""
        print("\n=== Case 4: 文件传输 ===")

        # 发送进入Case 4的命令
        # 原始命令: CMD = 4 (1字节)
        # 完整帧格式: 帧头(1) + 原始命令(1) + BCC(1) + 帧尾(1) = 4字节
        self.send_command(b'\x04')  # CMD = 4

        # 创建本地文件
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        filename = f"resistance_data_{timestamp}.txt"

        print(f"等待板子发送文件数据...")

        received_data = b""
        start_time = time.time()

        # 接收数据直到超时或收到结束标志
        while time.time() - start_time < 20:  # 20秒超时
            if self.serial_conn.in_waiting:
                data = self.serial_conn.read(self.serial_conn.in_waiting)
                received_data += data

                # 检查是否收到结束标志 (假设板子发送0xFF 0xFF作为结束标志)
                if received_data.endswith(b'\xFF\xFF'):
                    received_data = received_data[:-2]  # 移除结束标志
                    break

        if received_data:
            # 保存到本地文件
            with open(filename, 'wb') as f:
                f.write(received_data)

            print(f"文件已保存: {filename}")
            print(f"接收到的数据大小: {len(received_data)} 字节")

            # 显示文件内容
            print("\n文件内容:")
            try:
                content = received_data.decode('utf-8', errors='ignore')
                print(content)
            except:
                print("无法解码为文本内容")

        else:
            print("未收到文件数据")

    def main_menu(self):
        """主菜单"""
        while True:
            print("\n" + "="*50)
            print("上位机程序 - 板子通信控制")
            print("="*50)
            print("1. 参数调节 (Case 1)")
            print("2. 数据接收与解析 (Case 2)")
            print("3. 加密数据接收与解密 (Case 3)")
            print("4. 文件传输 (Case 4)")
            print("5. 退出程序")
            print("="*50)

            choice = input("请选择模式 (1-5): ").strip()

            if choice == '1':
                self.case1_parameter_adjustment()
            elif choice == '2':
                self.case2_data_receive()
            elif choice == '3':
                self.case3_encrypted_receive()
            elif choice == '4':
                self.case4_file_transfer()
            elif choice == '5':
                print("程序退出")
                break
            else:
                print("无效选择，请重新输入")

def main():
    # 创建上位机对象
    host = HostComputer(port='/dev/ttyUSB0', baudrate=9600)

    # 连接串口
    if not host.connect_serial():
        print("无法连接串口，程序退出")
        return

    try:
        # 显示主菜单
        host.main_menu()
    except KeyboardInterrupt:
        print("\n程序被用户中断")
    except Exception as e:
        print(f"程序运行出错: {e}")
    finally:
        # 断开串口连接
        host.disconnect_serial()

if __name__ == "__main__":
    main()
