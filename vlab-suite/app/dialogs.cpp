// dialogs.cpp -- small Windows helpers: file dialogs, folders, opening files in Excel / Explorer.
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include "app.hpp"

namespace vlab {

static int CALLBACK folderCallback(HWND h, UINT msg, LPARAM, LPARAM data) {
    if (msg == BFFM_INITIALIZED && data) SendMessageA(h, BFFM_SETSELECTIONA, TRUE, data);
    return 0;
}

std::string pickFolderDialog(const char* title, const std::string& initial) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    char path[MAX_PATH] = "";
    BROWSEINFOA bi{};
    bi.hwndOwner = GetActiveWindow();
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = folderCallback;
    bi.lParam = (LPARAM)initial.c_str();
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return std::string();
    bool ok = SHGetPathFromIDListA(pidl, path) != FALSE;
    CoTaskMemFree(pidl);
    return ok ? std::string(path) : std::string();
}

std::string openFileDialog(const char* filter, const char* initialDir) {
    char file[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initialDir;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    return GetOpenFileNameA(&ofn) ? std::string(file) : std::string();
}

std::string saveFileDialog(const char* filter, const char* defExt, const std::string& initialDir, const std::string& defaultName) {
    char file[MAX_PATH] = "";
    std::snprintf(file, sizeof file, "%s", defaultName.c_str());
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initialDir.c_str();
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    return GetSaveFileNameA(&ofn) ? std::string(file) : std::string();
}

std::string documentsFolder() {
    char path[MAX_PATH] = "";
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, path))) return path;
    const char* home = std::getenv("USERPROFILE");
    return home ? std::string(home) : std::string(".");
}

bool ensureDirectory(const std::string& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesA(path.c_str());
    if (a != INVALID_FILE_ATTRIBUTES) return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    size_t cut = path.find_last_of("\\/");
    if (cut != std::string::npos && cut > 2) ensureDirectory(path.substr(0, cut));
    return CreateDirectoryA(path.c_str(), nullptr) != 0;
}

void openInShell(const std::string& path, bool selectInExplorer) {
    if (selectInExplorer) {
        std::string arg = "/select,\"" + path + "\"";
        ShellExecuteA(nullptr, "open", "explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
    } else {
        ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

ImU32 argbToImU32(unsigned argb) {
    unsigned a = (argb >> 24) & 0xFF, r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF;
    return IM_COL32(r, g, b, a ? a : 255);
}
unsigned imU32ToArgb(ImU32 c) {
    unsigned r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF, a = (c >> 24) & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

}  // namespace vlab
