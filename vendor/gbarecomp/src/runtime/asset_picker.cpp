// asset_picker.cpp — see asset_picker.h.

#include "asset_picker.h"
#include "utf8_path.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../gba/crc32.h"
#include "../gba/sha1.h"
#include "host_platform.h"

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#endif

namespace fs = std::filesystem;

namespace gbarecomp {

namespace {

std::string exe_dir_from(const std::string& argv0) {
    if (argv0.empty()) return ".";
    fs::path p=path_from_utf8(argv0);
    if (p.has_parent_path()) {
        std::error_code ec;
        auto abs = fs::absolute(p, ec);
        if (!ec) return path_to_utf8(abs.parent_path());
        return path_to_utf8(p.parent_path());
    }
    return ".";
}

std::string cache_path(const std::string& exe_dir, const char* filename) {
    return path_to_utf8(path_from_utf8(exe_dir)/filename);
}

std::string read_cache(const std::string& path) {
    std::ifstream f(path_from_utf8(path));
    if (!f) return {};
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    return line;
}

void write_cache(const std::string& path, const std::string& asset_path) {
    std::ofstream f(path_from_utf8(path), std::ios::trunc);
    if (f) f << asset_path << "\n";
    if(!f)std::fprintf(stderr,"Could not save file selection; extract the ZIP to a writable folder.\n");
}

bool sha1_eq_lower(const std::string& got_hex, const char* expected) {
    if (!expected || !*expected) return true;
    std::size_t exp_len = std::strlen(expected);
    if (got_hex.size() != exp_len) return false;
    for (std::size_t i = 0; i < exp_len; ++i) {
        char a = static_cast<char>(std::tolower(
            static_cast<unsigned char>(got_hex[i])));
        char b = static_cast<char>(std::tolower(
            static_cast<unsigned char>(expected[i])));
        if (a != b) return false;
    }
    return true;
}

std::string hex32(std::uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08X", v);
    return std::string(buf);
}

#if defined(_WIN32)

// Win32 OPENFILENAMEA wants a buffer it can mutate; copy spec.dialog_filter
// (which is already double-null-terminated) into a stable vector.
std::vector<char> copy_filter(const char* filter) {
    std::vector<char> out;
    if (!filter) {
        const char* def = "All Files\0*.*\0";
        out.insert(out.end(), def, def + 14);
        out.push_back('\0');
        return out;
    }
    const char* p = filter;
    while (true) {
        std::size_t len = std::strlen(p);
        out.insert(out.end(), p, p + len);
        out.push_back('\0');
        p += len + 1;
        if (*p == '\0') {
            out.push_back('\0');
            break;
        }
    }
    return out;
}

std::string pick_with_dialog(const AssetSpec& spec) {
    const auto narrow_filter = copy_filter(spec.dialog_filter);
    std::vector<wchar_t> filter(narrow_filter.begin(),narrow_filter.end());
    std::wstring title=path_from_utf8(spec.dialog_title).wstring();
    wchar_t path_buf[32768] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter.data();
    ofn.lpstrFile = path_buf;
    ofn.nMaxFile = 32768;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return {};
    return path_to_utf8(fs::path(path_buf));
}

void show_dialog(const char* title, const std::string& body, UINT flags) {
    MessageBoxW(nullptr,path_from_utf8(body).c_str(),path_from_utf8(title).c_str(),flags);
}

#else

std::string pick_with_dialog(const AssetSpec&) { return {}; }
void show_dialog(const char*, const std::string&, unsigned) {}

#endif

AssetResult load_and_validate(const std::string& path,
                              const AssetSpec& spec) {
    AssetResult r;
    r.path = path;
    std::string err;
    std::error_code size_error;
    auto length=fs::file_size(path_from_utf8(path),size_error);
    if(size_error||length>64u*1024u*1024u||(spec.expected_size&&length!=spec.expected_size)){
        r.error=std::string(spec.display_name)+": missing file, wrong size, or exceeds 64 MiB limit";return r;
    }
    r.bytes = read_file(path, &err);
    if (!err.empty() || r.bytes.empty()) {
        r.error = err.empty() ? ("read failed: " + path) : err;
        return r;
    }
    if (spec.expected_size != 0 && r.bytes.size() != spec.expected_size) {
        std::ostringstream oss;
        oss << spec.display_name << " size mismatch: got "
            << r.bytes.size() << " bytes, expected "
            << spec.expected_size << " bytes.";
        r.error = oss.str();
        return r;
    }
    r.sha1_hex = gba::sha1(r.bytes.data(), r.bytes.size()).hex();
    r.crc32 = gba::crc32(r.bytes.data(), r.bytes.size());

    const bool sha_ok = sha1_eq_lower(r.sha1_hex, spec.expected_sha1);
    const bool crc_ok = (spec.expected_crc32 == 0) ||
                        (spec.expected_crc32 == r.crc32);

    if (!sha_ok || !crc_ok) {
        std::ostringstream w;
        w << spec.display_name << " hash mismatch:\n";
        w << "  SHA-1 got " << r.sha1_hex;
        if (spec.expected_sha1 && *spec.expected_sha1) {
            w << "\n        expected " << spec.expected_sha1;
        }
        w << "\n  CRC32 got " << hex32(r.crc32);
        if (spec.expected_crc32 != 0) {
            w << "\n        expected " << hex32(spec.expected_crc32);
        }
        if (spec.hash_mismatch_is_error) {
            r.error = w.str();
            return r;
        }
        w << "\n\nThis is not the recompiled-against image. Behavior "
             "is undefined; proceeding anyway.";
        r.warning = w.str();
    }
    r.ok = true;
    return r;
}

}  // namespace

AssetResult resolve_asset(const std::string& argv_path,
                          const AssetSpec& spec,
                          const std::string& argv0) {
    const std::string exe_dir = exe_dir_from(argv0);
    const std::string cache = cache_path(exe_dir, spec.cache_filename);

    // 1. Explicit argv path: if it loads and validates, take it (cache
    //    for next launch). If it doesn't even load (missing file / bad
    //    size), fall through to the cached path and the picker — the
    //    user may have typed the wrong path or be on a release build
    //    where the dev default doesn't exist.
    if (!argv_path.empty()) {
        std::error_code ec;
        if (fs::exists(path_from_utf8(argv_path), ec) && !ec) {
            auto r = load_and_validate(argv_path, spec);
            if (r.ok) {
                if (spec.persist_cache) write_cache(cache, argv_path);
                return r;
            }
            // Wrong size or unreadable. Don't bother the user with a
            // dialog if there isn't one available (non-Windows); they
            // need the diagnostic. On Windows, fall through to dialog.
#if !defined(_WIN32)
            return r;
#endif
        }
    }

    // 2. Cached path from a prior successful pick.
    if (spec.persist_cache) {
        const std::string cached = read_cache(cache);
        if (!cached.empty()) {
            std::error_code ec;
            if (fs::exists(path_from_utf8(cached), ec) && !ec) {
                auto r = load_and_validate(cached, spec);
                if (r.ok) return r;
            }
        }
    }

#if defined(_WIN32)
    while (true) {
        std::string picked = pick_with_dialog(spec);
        if (picked.empty()) {
            AssetResult r;
            r.error = std::string(spec.display_name) +
                      ": no file selected (dialog cancelled).";
            return r;
        }
        auto r = load_and_validate(picked, spec);
        if (!r.ok) {
            show_dialog(spec.display_name,
                        r.error + "\n\nPlease pick a different file.",
                        MB_OK | MB_ICONERROR);
            continue;
        }
        if (!r.warning.empty()) {
            show_dialog(spec.display_name, r.warning,
                        MB_OK | MB_ICONWARNING);
        }
        if (spec.persist_cache) write_cache(cache, picked);
        return r;
    }
#else
    AssetResult r;
    r.error = std::string(spec.display_name) +
              ": no path provided (use --bios / --rom on this platform).";
    return r;
#endif
}

AssetResult validate_asset_path(const std::string& path,
                                const AssetSpec& spec) {
    if (path.empty()) {
        AssetResult r;
        r.error = std::string(spec.display_name) + ": no file selected.";
        return r;
    }
    std::error_code ec;
    if (!fs::is_regular_file(path_from_utf8(path), ec) || ec) {
        AssetResult r;
        r.path = path;
        r.error = std::string(spec.display_name) +
                  ": file is missing or is not a regular file.";
        return r;
    }
    return load_and_validate(path, spec);
}

}  // namespace gbarecomp
