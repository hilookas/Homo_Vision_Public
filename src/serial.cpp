// 该文件为 libserial 的包装文件，转换 c++ 接口到 c 供 comm.c 使用

#include "serial.hpp"
#include <iostream>
#ifdef WITH_GNU_COMPILER
#include <libserial/SerialPort.h>
#endif

#ifdef WITH_GNU_COMPILER
static std::shared_ptr<LibSerial::SerialPort> _serial_port = nullptr;
#endif

extern "C" {

// 初始化串口
// 返回是否发生错误
bool serial_init(void) {
#ifdef WITH_GNU_COMPILER
    _serial_port = std::make_shared<LibSerial::SerialPort>();
    if (_serial_port == nullptr) {
        std::cerr << "Serial create failed." << std::endl;
        return true;
    }
    try {
        _serial_port->Open("/dev/ttyUSB0"); // 打开串口
        _serial_port->SetBaudRate(LibSerial::BaudRate::BAUD_460800); // 设置波特率
        _serial_port->SetCharacterSize(LibSerial::CharacterSize::CHAR_SIZE_8); // 8 位数据位
        _serial_port->SetFlowControl(LibSerial::FlowControl::FLOW_CONTROL_NONE); // 设置流控
        _serial_port->SetParity(LibSerial::Parity::PARITY_NONE); // 无校验
        _serial_port->SetStopBits(LibSerial::StopBits::STOP_BITS_1); // 1个停止位
    } catch (const LibSerial::OpenFailed &) {
        std::cerr << "Serial port open failed..." << std::endl;
        return true;
    } catch (const LibSerial::AlreadyOpen &) {
        std::cerr << "Serial port already open..." << std::endl;
        return true;
    } catch (...) {
        std::cerr << "Serial port exception..." << std::endl;
        return true;
    }
    return false;
#else
    return true;
#endif
}

// 堵塞输出一个字符
// c 为要发送的字符
// 返回是否发生错误
bool serial_send_blocking(uint8_t c) {
#ifdef WITH_GNU_COMPILER
    try {
        _serial_port->WriteByte(c); // 写数据到串口
        _serial_port->DrainWriteBuffer(); // 等待，直到写缓冲区耗尽，然后返回。
    } catch (const std::runtime_error &) {
        std::cerr << "The runtime_error." << std::endl;
        return true;
    } catch (const LibSerial::NotOpen &) {
        std::cerr << "Port Not Open..." << std::endl;
        return true;
    } catch (...) {
        std::cerr << "Serial port exception..." << std::endl;
        return true;
    }
    return false;
#else
    return true;
#endif
}

// 非堵塞轮询一个字符
// c 为要接受的字符
// 返回是否发生错误
bool serial_recv_poll(uint8_t *c) {
#ifdef WITH_GNU_COMPILER
    try {
        if (!_serial_port->IsDataAvailable()) {
            return true;
        }
        _serial_port->ReadByte(*c, 0); // 从串口读取一个数据
    } catch (const LibSerial::ReadTimeout &) {
        std::cerr << "The ReadByte() call has timed out." << std::endl;
        return true;
    } catch (const LibSerial::NotOpen &) {
        std::cerr << "Port Not Open ..." << std::endl;
        return true;
    } catch (...) {
        std::cerr << "Serial port exception..." << std::endl;
        return true;
    }
    return false;
#else
    return true;
#endif
}

}