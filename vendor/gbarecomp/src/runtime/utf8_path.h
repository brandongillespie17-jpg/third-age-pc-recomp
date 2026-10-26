#pragma once
#include <cstdio>
#include <filesystem>
#include <string>
namespace gbarecomp {
inline std::filesystem::path path_from_utf8(const std::string& s){
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()),s.size()));
}
inline std::string path_to_utf8(const std::filesystem::path& p){
    auto s=p.u8string();return std::string(reinterpret_cast<const char*>(s.data()),s.size());
}
inline std::FILE* fopen_utf8(const std::string& path,const char* mode){
#ifdef _WIN32
    std::wstring wide_mode;for(const char* p=mode;*p;++p)wide_mode.push_back(wchar_t(*p));
    return _wfopen(path_from_utf8(path).c_str(),wide_mode.c_str());
#else
    return std::fopen(path.c_str(),mode);
#endif
}
}
