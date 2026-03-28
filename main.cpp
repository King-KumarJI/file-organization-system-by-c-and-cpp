// main.cpp – Windows‑specific file organizer (no std::filesystem)

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <direct.h>   // _mkdir
#include <windows.h>  // Windows API
#include <io.h>       // _access
#include <memory>  // for unique_ptr

using namespace std;

// --- OS‑specific helpers ---

vector<string> list_files_recursive(const string& path) {
    vector<string> files;

    string pattern = path + "\\*";
    WIN32_FIND_DATAA data;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &data);

    if (hFind == INVALID_HANDLE_VALUE)
        return files;

    do {
        string fname = data.cFileName;

        if (fname == "." || fname == "..") continue;

        string full = path + "\\" + fname;

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            auto sub = list_files_recursive(full);
            files.insert(files.end(), sub.begin(), sub.end());
        } else {
            files.push_back(full);
        }
    } while (FindNextFileA(hFind, &data));

    FindClose(hFind);
    return files;
}

bool create_dir(const string& path) {
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
}

// --- File category rules ---

struct FileCategoryRules {
    map<string, string> ext_to_category;

    FileCategoryRules() {
        auto add = [&](initializer_list<string> exts, const string& cat) {
            for (auto& e : exts)
                ext_to_category[e] = cat;
        };

        add({"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff", "svg"}, "Images");
        add({"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm"}, "Videos");
        add({"txt", "pdf", "doc", "docx", "rtf", "odt", "ppt", "pptx", "xls", "xlsx", "csv"}, "Documents");
        add({"zip", "rar", "7z", "tar", "gz"}, "Archives");
    }

    string classify(const string& filename) const {
        auto pos = filename.rfind('.');
        string ext = pos == string::npos ? "no_ext" : filename.substr(pos + 1);
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        auto it = ext_to_category.find(ext);
        return it != ext_to_category.end() ? it->second : "Others";
    }
};

// --- Move file using Windows API ---

bool move_file(const string& src, const string& dst) {
    // Ensure parent dir exists
    auto slash = dst.rfind('\\');
    if (slash != string::npos) {
        string parent = dst.substr(0, slash);
        create_dir(parent);
    }

    // Convert to wide strings (Windows API uses UTF‑16)
    int src_w = MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, nullptr, 0);
    int dst_w = MultiByteToWideChar(CP_UTF8, 0, dst.c_str(), -1, nullptr, 0);

    if (src_w <= 0 || dst_w <= 0) return false;

    unique_ptr<wchar_t[]> src_buf(new wchar_t[src_w]);
    unique_ptr<wchar_t[]> dst_buf(new wchar_t[dst_w]);

    MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, src_buf.get(), src_w);
    MultiByteToWideChar(CP_UTF8, 0, dst.c_str(), -1, dst_buf.get(), dst_w);

    // Use MoveFileW (better than system("move"))
    if (!MoveFileW(src_buf.get(), dst_buf.get())) {
        DWORD err = GetLastError();
        // Optional: print error code
        // cerr << "MoveFile failed: " << err << " for " << src << " -> " << dst << '\n';
        return false;
    }

    return true;
}

// --- Generate unique target path ---

string make_unique_target(const string& src_dir, const string& dst_dir, const string& src) {
    auto last_slash = src.rfind('\\');
    string basename = src.substr(last_slash + 1);

    string stem, ext;
    auto dot = basename.rfind('.');
    if (dot != string::npos) {
        stem = basename.substr(0, dot);
        ext  = basename.substr(dot);
    } else {
        stem = basename;
        ext  = "";
    }

    string target = dst_dir + "\\" + basename;

    int counter = 1;
    while (_access(target.c_str(), 0) == 0) {
        ostringstream oss;
        oss << stem << "_" << counter << ext;
        target = dst_dir + "\\" + oss.str();
        ++counter;
    }

    return target;
}

// --- Main organization logic ---

void auto_organize(const string& root) {
    FileCategoryRules rules;
    vector<string> files = list_files_recursive(root);

    if (files.empty()) {
        cout << "No files found in: " << root << '\n';
        return;
    }

    map<string, int> counts;
    int moved = 0;

    for (const string& f : files) {
        string cat = rules.classify(f);
        counts[cat]++;

        string dst_dir = root + "\\" + cat;
        string dst = make_unique_target(root, dst_dir, f);

        if (move_file(f, dst)) {
            ++moved;
        } else {
            cerr << "FAILED to move: " << f << " -> " << dst << '\n';
        }
    }

    cout << "Organized files by category:\n";
    for (auto& p : counts) {
        cout << p.first << ": " << p.second << " file(s)\n";
    }
    cout << "Moved " << moved << " file(s).\n";
}

int main() {
    string path;
    cout << "Enter directory path to organize: ";
    getline(cin, path);

    // Normalize backslashes
    for (char& c : path) if (c == '/') c = '\\';

    // Ensure no trailing backslash
    if (!path.empty() && path.back() == '\\')
        path.pop_back();

    if (_access(path.c_str(), 0) != 0) {
        cerr << "Directory not found: " << path << '\n';
        return 1;
    }

    cout << "Starting automatic organization...\n";
    auto_organize(path);

    return 0;
}
