#include <cstdint>
// Portable release executes owned ROM/BIOS at runtime. No translated game code.
struct DispatchEntry { std::uint32_t addr; std::uint8_t thumb,resume; void (*fn)(void); };
extern "C" const DispatchEntry kDispatchTable[]={{0u,0u,0u,nullptr}};
extern "C" const unsigned kDispatchTableLen=0;
