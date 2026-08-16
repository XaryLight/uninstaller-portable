#include "systools.h"
#include <str/coding.h>
#include <base/qt.h>

ll informat::getsize(std::string& path){
    if (path.empty()) return 0;

    // 转换为宽字符路径以支持中文
    std::wstring wPath = utf8ToWide(path);

    // 跳过 UNC 网络路径（如 \\server\share）：Windows 对断连/慢速网络共享的
    // 文件访问会进行长达数十秒的重试，直接在主线程造成“卡死/未响应”。
    if (wPath.size() >= 2 && wPath[0] == L'\\' && wPath[1] == L'\\') return 0;
    // 仅扫描本地固定盘（DRIVE_FIXED）；跳过可移动盘/光驱/网络映射盘等，避免长耗时。
    if (wPath.size() >= 3 && wPath[1] == L':') {
        std::wstring root = wPath.substr(0, 3); // 形如 L"D:\\"
        UINT dt = GetDriveTypeW(root.c_str());
        if (dt != DRIVE_FIXED) return 0;
    }

    try {
        if (!fs::exists(wPath)) return 0;
    }
    catch (...) {
        return 0;
    }

    // 超大目录（如 Anaconda / 钉钉 数万文件）若不加限制会长时间阻塞主线程，
    // 导致列表构建期间界面“假死 / 窗口迟迟不出现”。这里用“文件数上限 / 时间预算 /
    // 总条目上限”三道闸限制遍历，并在迭代中定期泵 Qt 事件、跳过目录联接(junction)
    // 与符号链接目录（避免 AppData 等自引用造成的无限递归死循环），确保不会卡死。
    const ll kMaxFiles = 60000;      // 文件数上限
    const ll kTimeBudgetMs = 1000;   // 遍历时间预算（毫秒）
    const ll kMaxEntries = 200000;   // 总条目（文件+目录）上限，防止目录极深/极多导致耗时过长
    const ll kPumpInterval = 64;     // 每 64 个条目泵一次事件并检查时间
    ll totalSize = 0;
    ll fileCount = 0;
    ll entryCount = 0;
    auto start = std::chrono::steady_clock::now();
    try {
        for (auto it = fs::recursive_directory_iterator(wPath,
                 fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it) {
            const auto& entry = *it;
            // 目录联接(junction)/符号链接目录：不递归进入，防止无限遍历死循环。
            if (fs::is_directory(entry.symlink_status())) {
                DWORD attr = GetFileAttributesW(entry.path().c_str());
                if (attr != INVALID_FILE_ATTRIBUTES &&
                    (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
                    it.disable_recursion_pending();
                }
            }
            ++entryCount;
            if (fs::is_regular_file(entry.symlink_status())) {
                totalSize += fs::file_size(entry.path());
                if (++fileCount >= kMaxFiles) break;
            }
            // 定期泵事件 + 检查时间，避免在单个目录上连续阻塞导致“未响应”
            if ((entryCount & (kPumpInterval - 1)) == 0) {
                if (QCoreApplication::instance()) {
                    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                }
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if (ms >= kTimeBudgetMs) break;
            }
            if (entryCount >= kMaxEntries) break;
        }
    }
    catch (...) {
        // 忽略访问错误
    }
    return totalSize;
}
