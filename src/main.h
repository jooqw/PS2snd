#ifndef MAIN_H
#define MAIN_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <iostream>
#include <iomanip>

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;

inline void LogInfo(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

inline void LogErr(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

template<typename T>
inline void LogHex(const std::string& name, T val) {
    std::cout << "[DATA] " << name << ": 0x" << std::hex << (u32)val << std::dec << std::endl;
}

#endif // MAIN_H
