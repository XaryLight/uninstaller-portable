//
// Created by xubowen on 2026/6/7.
//
#include "mainwindow.h"
#include <res/version.h>


// 大小列专用表格项：按字节数（UserRole）排序，而不是按 "3.43 GB" / "344.02 MB" 文本排序。
class SizeTableItem : public QTableWidgetItem {
public:
    SizeTableItem(const QString& text, qint64 bytes)
        : QTableWidgetItem(text) {
        setData(Qt::UserRole, bytes);
        setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    bool operator<(const QTableWidgetItem& other) const override {
        qint64 a = data(Qt::UserRole).toLongLong();
        qint64 b = other.data(Qt::UserRole).toLongLong();
        return a < b;
    }
};

// 将 HICON 转换为 QPixmap（Qt 6 已无 QtWinExtras，这里用 GDI 自行转换）。
static QPixmap pixmapFromHICON(HICON hIcon) {
    if (!hIcon) return QPixmap();

    ICONINFO ii = { 0 };
    if (!GetIconInfo(hIcon, &ii)) return QPixmap();

    int w = 0, h = 0;
    BITMAP bmp = { 0 };
    if (ii.hbmColor) {
        GetObject(ii.hbmColor, sizeof(BITMAP), &bmp);
        w = bmp.bmWidth;
        h = bmp.bmHeight;
    }
    if ((w <= 0 || h <= 0) && ii.hbmMask) {
        GetObject(ii.hbmMask, sizeof(BITMAP), &bmp);
        w = bmp.bmWidth;
        h = bmp.bmHeight / 2;
    }
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (w <= 0 || h <= 0) {
        w = GetSystemMetrics(SM_CXICON);
        h = GetSystemMetrics(SM_CYICON);
    }

    // CreateCompatibleDC(nullptr) 在 Qt GUI 子系统下可能创建出无法用于 DIB 的 DC，
    // 改为基于屏幕 DC 创建，提高 ExtractIconEx 提取图标后转 QPixmap 的成功率。
    HDC screenDC = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);
    if (!dc) {
        return QPixmap();
    }

    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h; // 自顶向下
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    RGBQUAD* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS,
                                    reinterpret_cast<void**>(&bits), nullptr, 0);
    if (!hBmp) {
        DeleteDC(dc);
        return QPixmap();
    }
    HBITMAP hOld = reinterpret_cast<HBITMAP>(SelectObject(dc, hBmp));
    if (!DrawIconEx(dc, 0, 0, hIcon, w, h, 0, nullptr, DI_NORMAL)) {
        SelectObject(dc, hOld);
        DeleteObject(hBmp);
        DeleteDC(dc);
        return QPixmap();
    }
    SelectObject(dc, hOld);

    QImage img(w, h, QImage::Format_ARGB32);
    if (bits) {
        // 32-bit DIB（BI_RGB）在内存中的字节顺序为 BGRA，这与 little-endian
        // 下 QImage::Format_ARGB32 的字节顺序一致，直接拷贝即可，不要再 swap R/B。
        memcpy(img.bits(), bits, static_cast<size_t>(w) * h * 4);
    }
    DeleteObject(hBmp);
    DeleteDC(dc);
    return QPixmap::fromImage(img);
}

// 判断 QPixmap 是否实际可见（至少有一个像素的 alpha 不太低），
// 用于过滤掉 ExtractIconExW 偶尔返回的透明/无效图标，避免列表出现空白图标。
static bool pixmapIsVisible(const QPixmap& pm) {
    if (pm.isNull()) return false;
    QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    if (w <= 0 || h <= 0) return false;
    // 大图标（如 256x256 = 65536 像素）逐像素 qAlpha(img.pixel()) 太慢，改为用 scanLine
    // 直接读 alpha 字节；超过 16000 像素时再抽样扫描，兼顾速度与正确性。
    const int step = (w * h > 16000) ? 3 : 1;
    for (int y = 0; y < h; y += step) {
        const uchar* line = img.constScanLine(y);
        for (int x = 0; x < w; x += step) {
            // Format_ARGB32 内存布局为小端 BGRA，alpha 在第 4 字节（索引 3）
            if (line[x * 4 + 3] > 10) {
                return true;
            }
        }
    }
    return false;
}

// 获取一个彩色、确定可见的默认“应用程序”图标，作为无图标时的占位。
// 不依赖系统 SHGetStockIconInfo（它在某些主题/版本下实际返回文件图标），
// 而是自绘一个圆角彩色背景 + 白色应用剪影/名称缩写的图标，保证任何主题下都清晰可见。
// 传入软件名称时会根据名称哈希生成不同背景色，并绘制首字母缩写，使 VC++ 运行库这类无图标条目也能区分。
static QIcon defaultAppIcon(const QString& name = QString()) {
    QPixmap pm(20, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg(0x2D, 0x6C, 0xDF); // 默认蓝色
    QString text;
    if (!name.isEmpty()) {
        // 根据名称哈希挑选颜色，让不同软件显示不同底色。
        uint hash = 0;
        for (QChar c : name) {
            hash = hash * 31 + c.unicode();
        }
        static const QColor palette[] = {
            QColor(0x2D, 0x6C, 0xDF), QColor(0x28, 0xA7, 0x45),
            QColor(0xD9, 0x4A, 0x3C), QColor(0xF5, 0xA6, 0x23),
            QColor(0x8E, 0x44, 0xAD), QColor(0x17, 0xA2, 0xB8),
            QColor(0xE0, 0x5A, 0x8A), QColor(0x5D, 0x6D, 0x7E),
            QColor(0x27, 0xAE, 0x60), QColor(0xC0, 0x39, 0x2B),
            QColor(0xE6, 0x74, 0x00), QColor(0x6C, 0x5C, 0xE7)
        };
        bg = palette[hash % (sizeof(palette) / sizeof(palette[0]))];

        // 提取缩写：VC++ 相关统一显示“VC”，其他取前两个字母/数字字符。
        if (name.contains("Visual C++", Qt::CaseInsensitive) ||
            name.contains("VC++", Qt::CaseInsensitive)) {
            text = "VC";
        } else {
            for (QChar c : name) {
                if (c.isLetterOrNumber() && text.length() < 2) {
                    text.append(c.toUpper());
                }
            }
        }
    }

    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(1, 1, 18, 18), 4, 4);

    if (!text.isEmpty()) {
        p.setPen(Qt::white);
        QFont font = p.font();
        font.setPixelSize(text.length() == 1 ? 12 : 9);
        font.setBold(true);
        p.setFont(font);
        p.drawText(pm.rect(), Qt::AlignCenter, text);
    } else {
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(6.5, 5, 7, 7));
        p.drawRoundedRect(QRectF(6.5, 13, 7, 3.5), 1.75, 1.75);
    }
    p.end();
    return QIcon(pm);
}

static QString stripQuotes(QString s) {
    if ((s.startsWith('"') && s.endsWith('"')) ||
        (s.startsWith('\'') && s.endsWith('\''))) {
        s = s.mid(1, s.size() - 2);
    }
    return s;
}

// 去掉卸载命令行里 exe 路径两侧的引号（注册表常见格式："C:\\Path\\uninst.exe" /param）。
// 只处理第一个参数（即 exe 路径）的成对引号，保留后续参数中的引号，用于 UI 显示。
static QString stripCommandQuotes(QString s) {
    s = s.trimmed();

    // 情况 1：整个命令首尾被同一对引号包围（如 "C:\\a.exe /S"）
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.mid(1, s.size() - 2);
        return s.trimmed();
    }

    // 情况 2：仅第一个参数（exe 路径）被引号包围，后面还有参数。
    // 例如 "C:\\Path\\uninst.exe" /S
    if (s.startsWith('"')) {
        int close = s.indexOf('"', 1);
        if (close > 1 && (close + 1 == s.size() || s[close + 1].isSpace())) {
            s.remove(close, 1);
            s.remove(0, 1);
        }
    }
    return s;
}

static QIcon iconFromFile(const QString& filePath, int index = 0) {
    if (filePath.isEmpty()) return QIcon();

    // 注册表 DisplayIcon 常指向独立的 .ico/.png 图片文件，
    // ExtractIconExW 对这类文件通常返回 0，直接用 QPixmap/QIcon 加载。
    QString lower = filePath.toLower();
    if (lower.endsWith(".ico") || lower.endsWith(".png") ||
        lower.endsWith(".bmp") || lower.endsWith(".jpg") ||
        lower.endsWith(".jpeg") || lower.endsWith(".webp")) {
        QPixmap pm(filePath);
        if (!pm.isNull()) {
            pm = pm.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            return QIcon(pm);
        }
    }

    std::wstring wPath = QDir::toNativeSeparators(filePath).toStdWString();
    // 优先提取大图标再缩放，颜色/细节比 16x16 小图标好很多。
    HICON hIconLarge = nullptr;
    HICON hIcon = nullptr;
    ExtractIconExW(wPath.c_str(), index, &hIconLarge, nullptr, 1);
    if (hIconLarge) {
        hIcon = hIconLarge;
    } else {
        ExtractIconExW(wPath.c_str(), index, nullptr, &hIcon, 1);
    }
    if (hIcon) {
        QPixmap pm = pixmapFromHICON(hIcon);
        DestroyIcon(hIcon);
        if (pixmapIsVisible(pm)) {
            pm = pm.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            return QIcon(pm);
        }
        // 大图标转换失败/不可见时退而尝试小图标（某些图标的资源索引只存了小图标）
        HICON hIconSmall = nullptr;
        ExtractIconExW(wPath.c_str(), index, nullptr, &hIconSmall, 1);
        if (hIconSmall) {
            QPixmap pmSmall = pixmapFromHICON(hIconSmall);
            DestroyIcon(hIconSmall);
            if (pixmapIsVisible(pmSmall)) {
                pmSmall = pmSmall.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                return QIcon(pmSmall);
            }
        }
    }
    return QIcon();
}

// 从 SoftwareInfo::displayIcon（如 "C:\app.exe" 或 "C:\app.dll,-123"）提取图标。
// DisplayIcon 经常被加上双引号，提取前先去掉，否则 ExtractIconExW 会失败。
// 从 SoftwareInfo::displayIcon 提取图标；缺失或失败时再尝试安装目录 exe、
// 卸载命令 exe，最后回退到确定可见的默认应用图标，避免列表出现空白图标。
static QString extractExePath(const QString& cmd); // 前向声明（定义在文件下方）

// 通过顶层窗口标题查找正在运行的进程 exe 路径，用于注册表路径过期时的图标回退。
static QString findRunningExeByWindowTitle(const QStringList& nameKeys) {
    if (nameKeys.isEmpty()) return QString();

    struct EnumCtx {
        QStringList keys;
        QString result;
    };
    EnumCtx ctx = { nameKeys, QString() };

    auto enumProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hwnd)) return TRUE;
        wchar_t title[256] = { 0 };
        GetWindowTextW(hwnd, title, 256);
        if (!title[0]) return TRUE;
        QString t = QString::fromWCharArray(title);
        EnumCtx* c = reinterpret_cast<EnumCtx*>(lParam);
        for (const QString& key : c->keys) {
            if (!key.isEmpty() && t.contains(key, Qt::CaseInsensitive)) {
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                if (pid) {
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                    if (hProc) {
                        wchar_t path[MAX_PATH] = { 0 };
                        DWORD size = MAX_PATH;
                        if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
                            c->result = QString::fromWCharArray(path);
                        }
                        CloseHandle(hProc);
                    }
                }
                return FALSE; // 找到即停止
            }
        }
        return TRUE;
    };

    EnumWindows(enumProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.result;
}

static QIcon iconForSoftware(const SoftwareInfo* sw) {
    if (!sw) return defaultAppIcon();

    QString displayName = QString::fromStdString(sw->displayName);

    // 1) 优先用 DisplayIcon（可能带引号 / 资源ID）
    if (!sw->displayIcon.empty()) {
        QString raw = QString::fromStdString(sw->displayIcon).trimmed();
        int comma = raw.lastIndexOf(',');
        int index = 0;
        QString path = raw;
        if (comma != -1) {
            bool ok = false;
            int idx = raw.mid(comma + 1).toInt(&ok);
            if (ok) {
                index = idx;
                path = raw.left(comma);
            }
        }
        path = stripQuotes(path).trimmed();
        QIcon ico = iconFromFile(path, index);
        if (!ico.isNull()) return ico;
    }

    // 2) DisplayIcon 缺失或失败：从安装目录里找 exe 取图标（递归两层，
    //    因为有些软件如 Adobe Acrobat 把主程序放在安装目录子文件夹里）。
    //    优先匹配与软件名 / Publisher 相关的 exe，避免把 auclt.exe 这类
    //    辅助程序的图标当成主程序图标（如腾讯QQ误显示成地球图标）。
    QString installLoc = stripQuotes(QString::fromStdString(sw->installLocation)).trimmed();
    if (!installLoc.isEmpty()) {
        QDir dir(installLoc);
        if (dir.exists()) {
            struct Candidate {
                QString path;
                QString fileName;
                int depth;
                int score;
            };
            QList<Candidate> candidates;

            QDirIterator it(installLoc, QStringList() << "*.exe",
                            QDir::Files, QDirIterator::Subdirectories);
            const int maxDepth = 2;
            while (it.hasNext()) {
                QString fp = it.next();
                QString rel = fp.mid(installLoc.length());
                rel = rel.replace('\\', '/');
                int depth = rel.count('/');
                if (depth > maxDepth) continue;
                candidates.append({fp, QFileInfo(fp).fileName(), depth, 0});
            }

            auto collectKeys = [](const QString& s) -> QStringList {
                QStringList keys;
                QString base = s;
                int paren = base.indexOf('(');
                if (paren != -1) base = base.left(paren);
                base = base.trimmed().toLower();
                keys << base;
                QRegularExpression re("[a-z0-9]+");
                auto mi = re.globalMatch(base);
                while (mi.hasNext()) {
                    QString w = mi.next().captured();
                    if (w.length() >= 2) keys << w;
                }
                return keys;
            };

            QStringList softKeys = collectKeys(displayName);
            QString publisher = QString::fromStdString(sw->publisher).trimmed();
            QStringList pubKeys = collectKeys(publisher);

            for (auto& c : candidates) {
                QString fnBase = QFileInfo(c.fileName).baseName().toLower();
                for (const QString& k : softKeys) {
                    if (k.isEmpty()) continue;
                    if (fnBase == k) {
                        c.score += 200; // 文件名完全匹配软件名关键字：最高优先级
                    } else if (fnBase.contains(k)) {
                        c.score += 100;
                    }
                }
                for (const QString& k : pubKeys) {
                    if (k.isEmpty()) continue;
                    if (fnBase == k) {
                        c.score += 150;
                    } else if (fnBase.contains(k)) {
                        c.score += 50;
                    }
                }
                c.score -= c.depth; // 同分则深度更浅优先
            }

            std::sort(candidates.begin(), candidates.end(),
                      [](const Candidate& a, const Candidate& b) {
                          return a.score > b.score;
                      });

            for (const auto& c : candidates) {
                QIcon ico = iconFromFile(c.path, 0);
                if (!ico.isNull()) return ico;
            }
        }
    }

    // 3) 仍没有：从卸载命令里解析出的 exe 取图标
    QString cmd = QString::fromStdString(Registry::getUninstallCommand(*sw));
    QString exe = extractExePath(cmd);
    if (!exe.isEmpty() && QFile::exists(exe)) {
        QIcon ico = iconFromFile(exe, 0);
        if (!ico.isNull()) return ico;
    }

    // 4) 仍没有：如果软件正在运行，从运行中主窗口对应的进程 exe 提取图标。

    // 4) 仍没有：如果软件正在运行，从运行中主窗口对应的进程 exe 提取图标。
    // 这能处理注册表 InstallLocation/DisplayIcon 已过期，但程序实际装在别处的情况
    // （典型例子：微信注册表指向 WeChat\WeChat.exe，实际运行进程是 Weixin\Weixin.exe）。
    {
        QStringList nameKeys;
        nameKeys << displayName;
        if (displayName.contains("微信", Qt::CaseInsensitive) ||
            displayName.contains("WeChat", Qt::CaseInsensitive) ||
            displayName.contains("Weixin", Qt::CaseInsensitive)) {
            nameKeys << "微信" << "Weixin" << "WeChat";
        }
        QString runningExe = findRunningExeByWindowTitle(nameKeys);
        if (!runningExe.isEmpty() && QFile::exists(runningExe)) {
            QIcon ico = iconFromFile(runningExe, 0);
            if (!ico.isNull()) return ico;
        }
    }

    // 5) 某些软件（如微信）主程序目录已被清空，但 C:\ProgramData\Publisher\Product
    // 下仍有带图标的辅助程序（如 WeChatUpdate.exe）。根据 Publisher + 软件名生成常见
    // 路径组合尝试提取，避免已卸载残留项显示完全无关的默认图标。
    {
        QString publisher = QString::fromStdString(sw->publisher).trimmed();
        if (!publisher.isEmpty() && !displayName.isEmpty()) {
            // 清理 Publisher：去掉括号及以后内容
            QString pubBase = publisher;
            int paren = pubBase.indexOf('(');
            if (paren != -1) pubBase = pubBase.left(paren);
            pubBase = pubBase.trimmed();

            // 清理软件名：去掉版本号等括号内容
            QString nameBase = displayName.trimmed();
            paren = nameBase.indexOf('(');
            if (paren != -1) nameBase = nameBase.left(paren);
            nameBase = nameBase.trimmed();

            QStringList pubNames;
            pubNames << pubBase;
            if (pubBase.contains("腾讯", Qt::CaseInsensitive)) {
                pubNames << "Tencent";
            }

            QStringList prodNames;
            prodNames << nameBase;
            if (nameBase.contains("微信", Qt::CaseInsensitive) ||
                nameBase.contains("WeChat", Qt::CaseInsensitive)) {
                prodNames << "WeChat" << "微信";
            }

            for (const QString& pub : std::as_const(pubNames)) {
                for (const QString& prod : std::as_const(prodNames)) {
                    QString base = "C:/ProgramData/" + pub + "/" + prod;
                    QDir pd(base);
                    if (!pd.exists()) continue;
                    QFileInfoList exes = pd.entryInfoList(QStringList() << "*.exe",
                                                          QDir::Files | QDir::NoDotAndDotDot,
                                                          QDir::Name);
                    for (const QFileInfo& fi : exes) {
                        QIcon ico = iconFromFile(fi.absoluteFilePath(), 0);
                        if (!ico.isNull()) return ico;
                    }
                }
            }
        }
    }

    // 6) 兜底：确定可见的默认应用图标（按软件名称生成带缩写的彩色图标）
    return defaultAppIcon(displayName);
}

// 深色主题：主界面黑底、浅灰文字，表格/按钮/搜索框均有明显边界，避免内容看不清。
static const char* kAppStyleSheet = R"(
    QMainWindow { background-color: #181a1e; }
    QWidget#centralWidget { background-color: #181a1e; }

    QMenuBar { background-color: #202328; padding: 2px; color: #e0e3e8; }
    QMenuBar::item { padding: 4px 10px; border-radius: 4px; color: #e0e3e8; }
    QMenuBar::item:selected { background-color: #353a43; }
    QMenuBar::item:pressed { background-color: #414752; }
    QMenu { background-color: #202328; color: #e0e3e8; border: 1px solid #3c424d; }
    QMenu::item:selected { background-color: #353a43; }
    QMenu::separator { background-color: #3c424d; height: 1px; margin: 4px 8px; }

    QLabel { font-size: 13px; color: #c9cdd3; }

    QLineEdit {
        border: 1px solid #3c424d;
        border-radius: 6px;
        padding: 6px 10px;
        font-size: 13px;
        background-color: #202328;
        color: #e8eaed;
    }
    QLineEdit:focus { border: 1px solid #5c9ad6; }
    QLineEdit::placeholder { color: #7f8794; }

    QPushButton {
        background-color: #4a90d9;
        color: #ffffff;
        border: none;
        border-radius: 6px;
        padding: 7px 16px;
        font-size: 13px;
    }
    QPushButton:hover { background-color: #5c9fe6; }
    QPushButton:pressed { background-color: #3a7fc7; }

    QPushButton#uninstallBtn { background-color: #d64a3c; }
    QPushButton#uninstallBtn:hover { background-color: #e25b4d; }
    QPushButton#uninstallBtn:pressed { background-color: #b84034; }

    QPushButton#scanBtn,
    QPushButton#detailsBtn,
    QPushButton#refreshBtn { background-color: #535b66; }
    QPushButton#scanBtn:hover,
    QPushButton#detailsBtn:hover,
    QPushButton#refreshBtn:hover { background-color: #616b78; }
    QPushButton#scanBtn:pressed,
    QPushButton#detailsBtn:pressed,
    QPushButton#refreshBtn:pressed { background-color: #454c56; }

    QTableWidget {
        background-color: #202328;
        alternate-background-color: #262a31;
        gridline-color: #323842;
        border: 1px solid #3c424d;
        border-radius: 8px;
        font-size: 13px;
        color: #e0e3e8;
        selection-background-color: #2f4a66;
        selection-color: #ffffff;
    }
    QTableWidget::item {
        padding: 5px 6px;
        color: #e0e3e8;
        background-color: transparent;
    }
    QTableWidget::item:selected {
        background-color: #2f4a66;
        color: #ffffff;
    }
    QHeaderView::section {
        background-color: #2b3038;
        color: #c9cdd3;
        padding: 8px 6px;
        border: none;
        border-bottom: 2px solid #4a525e;
        font-weight: 600;
    }
    QHeaderView::section:horizontal { border-right: 1px solid #3c424d; }

    QStatusBar { background-color: #202328; color: #9da3ad; }
    QStatusBar::item { border: none; }
    QDialog { background-color: #181a1e; }
    QTextEdit {
        background-color: #202328;
        border: 1px solid #3c424d;
        border-radius: 6px;
        color: #e0e3e8;
        font-size: 12px;
    }
    QCheckBox { color: #c9cdd3; }
    QMessageBox { background-color: #202328; }
)";


UninstallerWindow::UninstallerWindow(QWidget* parent) : QMainWindow(parent) {}

void UninstallerWindow::updateFindList() {
    findlist.clear();
    findlist.push_back(0); // Normal
    findlist.push_back(1); // WindowsInstaller
    if (m_showSystemComponents) {
        findlist.push_back(2); // SystemComponent
    }
}

SoftwareInfo* UninstallerWindow::softwareAtRow(int row) const {
    if (row < 0 || row >= m_tableWidget->rowCount()) return nullptr;
    QTableWidgetItem* item = m_tableWidget->item(row, 0);
    if (!item) return nullptr;
    return reinterpret_cast<SoftwareInfo*>(item->data(Qt::UserRole).value<qintptr>());
}

void UninstallerWindow::run() {
    // 设置窗口标题栏与任务栏图标（exe 内嵌的 IDI_APP_ICON）
    setWindowIcon(QApplication::windowIcon());
    setupUI();
    built_list();
    loadSoftwareList();
    show();
    showUpdatePopup();   // 启动即弹出更新日志
}

void UninstallerWindow::fresh() {
    setupUI();
    show();
}

void UninstallerWindow::built_list() {
    m_softwareList = Registry::getAllInstalledSoftware();
    len = m_softwareList.size();
    total_size = 0;
    QProgressDialog progress(
        tr("Scanning software ..."),
        tr("Clean"),
        0,
        len,
        this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(1000);
    for (int i = 0; i < len; i++) {
        if (progress.wasCanceled()) {
            m_softwareList.clear();
            len = 0;
            break;
        }
        auto sw = &m_softwareList[i];
        sw->registryInit();
        total_size += sw->size.size;
        progress.setLabelText(G.TRANSLATOR->tr("Loading: %1, %2/%3 - found: %4")
            .arg(QString::fromStdString(sw->displayName))
            .arg(i)
            .arg(len)
            .arg(QString::fromStdString(total_size.get())));
        progress.setValue(i);
        // 每次处理完一个软件都泵一次事件，确保进度条/标题栏不会长时间显示“未响应”。
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    progress.close();
}

void UninstallerWindow::sorting() {
    m_swlist.clear();
    int len = m_softwareList.size();
    for (int i = 0; i < len; i++) {
        auto sw = &m_softwareList[i];
        if (sw->isRunningTime)m_swlist[3].push_back(sw);
        else if (sw->isSystemComponent)m_swlist[2].push_back(sw);
        else if (sw->isWindowsInstaller)m_swlist[1].push_back(sw);
        else if (sw->displayName.empty())m_swlist[4].push_back(sw);
        else m_swlist[0].push_back(sw);
    }

}

void UninstallerWindow::loadSoftwareList() {
    updateFindList();
    sorting();
    filesize_t ts = 0;
    int i{ 0 }, n{ 0 };
    QProgressDialog progress(
        tr("Loading: %1, %2/%3 - found: %4"),
        tr("Clean"),
        0,
        len,
        this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    m_tableWidget->setSortingEnabled(false);
    m_tableWidget->setRowCount(len);
    for (const auto j : m_swlist) {
        if (progress.wasCanceled()) break;
        //if (!func::similarly(j.first, this->findlist))continue;
        for (const auto sw : j.second) {
            auto* nameItem = new QTableWidgetItem(QString::fromStdString(sw->displayName));
            nameItem->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<qintptr>(sw)));
            nameItem->setIcon(iconForSoftware(sw));
            m_tableWidget->setItem(i, 0, nameItem);

            m_tableWidget->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(sw->displayVersion)));
            m_tableWidget->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(sw->installDate)));

            auto* sizeItem = new SizeTableItem(
                QString::fromStdString(sw->size.get()),
                static_cast<qint64>(sw->size.size));
            m_tableWidget->setItem(i, 3, sizeItem);

            m_tableWidget->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(sw->publisher)));
            m_tableWidget->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(sw->installLocation)));

            // 状态列：残留项标记红色“残留”
            if (sw->isOrphaned) {
                auto* statusItem = new QTableWidgetItem(QString::fromUtf8(u8"残留"));
                QFont sf = statusItem->font();
                sf.setBold(true);
                statusItem->setFont(sf);
                statusItem->setForeground(QColor(0xE0, 0x5A, 0x5A));
                m_tableWidget->setItem(i, 6, statusItem);
                // 名称也染成警告色，方便一眼识别
                QFont nf = nameItem->font();
                nameItem->setForeground(QColor(0xD9, 0x7A, 0x7A));
                nameItem->setFont(nf);
            } else {
                m_tableWidget->setItem(i, 6, new QTableWidgetItem(QString::fromUtf8(u8"正常")));
            }

            ts += sw->size.size;
            i++, n++;
            progress.setValue(i);
            progress.setLabelText(QString("%1 (%2/%3)")
                .arg(QString::fromStdString(sw->displayName))
                .arg(n)
                .arg(len));
            if ((n & 0x1F) == 0) {
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
            if (progress.wasCanceled()) break;
        }
    }
    m_tableWidget->setRowCount(i);
    m_tableWidget->resizeColumnsToContents();
    m_tableWidget->setSortingEnabled(true);
    m_tableWidget->sortItems(0, Qt::AscendingOrder);
    progress.close();

    // 状态栏统一由 filterSoftware 更新为可见行数/可见大小；
    // 若搜索框为空则显示全部，非空则保持过滤状态。
    filterSoftware();
}

bool UninstallerWindow::tick(const ll& row) {
    if (!softwareAtRow(row)) {
        QMessageBox::warning(this, tr("Tip"), tr("Please select an application"));
        return 1;
    }
    return 0;
}

void UninstallerWindow::uninstallSelected() {
    int row = m_tableWidget->currentRow();
    if (tick(row))return ;

    auto software = softwareAtRow(row);
    if (!software) return;

    QString name = QString::fromStdString(software->displayName);
    QString ver = QString::fromStdString(software->displayVersion);
    QString cmd = QString::fromStdString(Registry::getUninstallCommand(*software));

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(G.TRANSLATOR->tr("Ensure uninstaller"));
    box.setText(
        tr("Sure to uninstaller the following software?\n\nSoftware: %1 Version: %2\n\nNote: It may take a while to uninstall, please wait patiently.")
        .arg(name)
        .arg(ver));
    if (!cmd.isEmpty()) {
        box.setInformativeText(G.TRANSLATOR->tr("Command to execute:") + "\n" + stripCommandQuotes(cmd));
    }
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes) return;

    if (m_busy) return;
    m_busy = true;

    QProgressDialog progress(G.TRANSLATOR->tr("Uninstalling %1 ...").arg(name),
        tr("Clean"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();
    QApplication::processEvents();

    bool success = Registry::uninstallSoftware(*software);
    progress.close();

    if (success) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this, tr("Successful"), tr("Software uninstalled successfully.\n\nScan residual files?"),
            QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::Yes) {
            scanResiduals();
        }
        // 重新扫描注册表，让列表反映真实状态（已卸载的条目会消失）
        built_list();
        loadSoftwareList();
    }
    else {
        QMessageBox::critical(this, tr("Fail"), tr("Uninstall fail"));
    }

    m_busy = false;
}

bool UninstallerWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_tableWidget->viewport() && event->type() == QEvent::ContextMenu) {
        auto* ctx = static_cast<QContextMenuEvent*>(event);
        onTableContextMenu(ctx->pos());
        return true; // 已处理，不再走默认菜单
    }
    return QMainWindow::eventFilter(obj, event);
}

void UninstallerWindow::onTableContextMenu(const QPoint& pos) {
    QTableWidgetItem* item = m_tableWidget->itemAt(pos);
    if (!item) return;
    int row = item->row();
    if (!softwareAtRow(row)) return;

    QMenu menu(this);
    QAction* actUninstall = menu.addAction(G.TRANSLATOR->tr("Uninstall selected software"));
    QAction* actScan = menu.addAction(G.TRANSLATOR->tr("Scan files"));
    QAction* actDetail = menu.addAction(G.TRANSLATOR->tr("Look detail"));
    QAction* actOpen = menu.addAction(G.TRANSLATOR->tr("Open file location"));
    menu.addSeparator();
    QAction* actCopy = menu.addAction(G.TRANSLATOR->tr("Copy uninstall command"));
    QAction* actDelReg = menu.addAction(QString::fromUtf8(u8"删除残留注册表项"));
    QAction* actForceDel = menu.addAction(QString::fromUtf8(u8"强制删除此条目"));

    // 选中右键所在行，再触发与按钮相同的逻辑
    connect(actUninstall, &QAction::triggered, this, [this, row]() {
        m_tableWidget->setCurrentCell(row, 0);
        uninstallSelected();
    });
    connect(actScan, &QAction::triggered, this, [this, row]() {
        m_tableWidget->setCurrentCell(row, 0);
        scanResiduals();
    });
    connect(actDetail, &QAction::triggered, this, [this, row]() {
        m_tableWidget->setCurrentCell(row, 0);
        showDetails();
    });
    connect(actOpen, &QAction::triggered, this, [this, row]() {
        m_tableWidget->setCurrentCell(row, 0);
        openFileLocation();
    });
    connect(actCopy, &QAction::triggered, this, [this, row]() {
        m_tableWidget->setCurrentCell(row, 0);
        copyUninstallCommand();
    });
    connect(actDelReg, &QAction::triggered, this, [this, row]() {
        m_tableWidget->setCurrentCell(row, 0);
        deleteRegistryEntry();
    });
    connect(actForceDel, &QAction::triggered, this, [this, row]() {
        m_tableWidget->setCurrentCell(row, 0);
        forceDeleteEntry();
    });

    menu.exec(m_tableWidget->viewport()->mapToGlobal(pos));
}

void UninstallerWindow::scanResiduals() {
    int row = m_tableWidget->currentRow();
    if (tick(row))return;

    auto software = softwareAtRow(row);

    QProgressDialog progress(
        tr("Scanning files ..."),
        tr("Clean"),
        0,
        0,
        this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QApplication::processEvents();

    auto residuals = Registry::scanResidualFiles(*software);
    progress.close();

    if (residuals.empty()) {
        QMessageBox::information(this,
            tr("Scan result"),
            tr("Not found")
            );
        return;
    }

    // 显示残留文件列表
    QDialog dialog(this);
    dialog.setWindowTitle(G.TRANSLATOR->tr("File list"));
    dialog.resize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QTextEdit* textEdit = new QTextEdit(&dialog);
    textEdit->setReadOnly(true);

    QString content;
    content += tr("Software: %1\n\n").arg(QString::fromStdString(software->displayName));
    content += tr("Found %1 files: \n\n").arg(residuals.size());

    for (const auto& file : residuals) {
        content += QString::fromStdString(file) + "\n";
    }

    textEdit->setText(content);
    layout->addWidget(textEdit);

    QPushButton* deleteBtn = new QPushButton(G.TRANSLATOR->tr("Delete all"), &dialog);
    QPushButton* cancelBtn = new QPushButton(G.TRANSLATOR->tr("Clean"), &dialog);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(deleteBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(deleteBtn, &QPushButton::clicked, [&]() {
        QMessageBox::StandardButton confirm = QMessageBox::question(
            &dialog,
            QString::fromUtf8(u8"确认删除残留文件"),
            QString::fromUtf8(u8"即将删除上面列出的 %1 个残留文件/目录，此操作不可撤销。\n确定继续吗？").arg(residuals.size()),
            QMessageBox::Yes | QMessageBox::No);
        if (confirm != QMessageBox::Yes) return;
        if (Registry::deleteResidualFiles(residuals)) {
            QMessageBox::information(&dialog, tr("Successful"), tr("Deletion completed!"));
            dialog.accept();
        }
        else {
            QMessageBox::warning(&dialog, tr("Fail"), tr("Some delete fail."));
        }
        });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

void UninstallerWindow::deleteRegistryEntry() {
    int row = m_tableWidget->currentRow();
    if (tick(row)) return;

    auto sw = softwareAtRow(row);
    if (!sw) return;

    QString name = QString::fromStdString(sw->displayName);
    QString regPath = QString::fromStdString(sw->orgPath);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QString::fromUtf8(u8"删除残留注册表项"));
    box.setText(QString::fromUtf8(u8"确定要从注册表删除该条目吗？\n\n软件: %1\n注册表位置: %2").arg(name).arg(regPath));
    box.setInformativeText(QString::fromUtf8(u8"此操作会直接移除该软件的注册表卸载项（适用于已卸载/残留的软件）。删除后无法撤销，且不会删除任何磁盘文件。"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes) return;

    if (m_busy) return;
    m_busy = true;
    bool ok = Registry::deleteRegistryKey(sw->hive, sw->regPath);
    m_busy = false;

    if (ok) {
        QMessageBox::information(this, QString::fromUtf8(u8"成功"), QString::fromUtf8(u8"注册表项已删除。"));
        // 重新扫描注册表，让列表反映真实状态（残留条目会消失）
        built_list();
        loadSoftwareList();
    }
    else {
        QMessageBox::critical(this, QString::fromUtf8(u8"失败"),
            QString::fromUtf8(u8"删除注册表项失败。\n若为 HKLM 项，可能被安全软件拦截或需要管理员权限；若为 HKCU 项，则该项可能已被删除。"));
    }
}

// 强制删除此条目：绕过残留自动检测，直接移除注册表卸载项，并强制扫描/删除其磁盘残留。
// 适用于“程序打不开/想强制当残留删”的场景。操作不可撤销，故二次确认 + 明确风险提示。
void UninstallerWindow::forceDeleteEntry() {
    int row = m_tableWidget->currentRow();
    if (tick(row)) return;

    auto sw = softwareAtRow(row);
    if (!sw) return;

    QString name = QString::fromStdString(sw->displayName);
    QString regPath = QString::fromStdString(sw->orgPath);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle(QString::fromUtf8(u8"强制删除此条目"));
    box.setText(QString::fromUtf8(u8"⚠️ 即将强制删除该软件条目：\n\n软件: %1\n注册表位置: %2").arg(name).arg(regPath));
    box.setInformativeText(QString::fromUtf8(u8"此操作会：\n"
        "1) 直接从注册表移除该软件的卸载项；\n"
        "2) 强制扫描并删除其残留/安装目录（含 AppData、ProgramData、开始菜单下以软件名命名的目录）。\n\n"
        "该软件当前未被判定为“残留”（文件可能仍在使用中），强制删除可能误删正在使用的软件数据，且操作不可撤销。确定继续吗？"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes) return;

    if (m_busy) return;
    m_busy = true;

    // 1) 删除注册表项
    bool okReg = Registry::deleteRegistryKey(sw->hive, sw->regPath);

    // 2) 强制扫描并删除磁盘残留（绕过 isOrphaned 判断）
    auto residuals = Registry::scanResidualFiles(*sw, /*force=*/true);
    bool okFiles = true;
    if (!residuals.empty()) {
        okFiles = Registry::deleteResidualFiles(residuals);
    }

    m_busy = false;

    if (okReg) {
        QString extra;
        if (!residuals.empty()) {
            extra = QString::fromUtf8(u8"\n已一并删除 %1 个磁盘残留/目录。").arg(residuals.size());
        }
        else {
            extra = QString::fromUtf8(u8"\n未发现可删除的磁盘残留目录。");
        }
        if (!okFiles) extra += QString::fromUtf8(u8"\n（部分残留文件删除失败，可能正被占用或无权限）");
        QMessageBox::information(this, QString::fromUtf8(u8"已完成"),
            QString::fromUtf8(u8"已强制删除该软件条目。%1").arg(extra));
        // 重新扫描注册表，让列表反映真实状态（该条目会消失）
        built_list();
        loadSoftwareList();
    }
    else {
        QMessageBox::critical(this, QString::fromUtf8(u8"失败"),
            QString::fromUtf8(u8"删除注册表项失败。\n若为 HKLM 项，可能被安全软件拦截或需要管理员权限；若为 HKCU 项，则该项可能已被删除。"));
    }
}

// 卸载本程序：从自身安装目录找到随附的 uninst.exe（纯 Win32 卸载桩），
// 启动它之后本程序立即退出，由 uninst.exe 负责删除安装目录、快捷方式与注册表项，
// 从而规避“删正在运行的自己”的自锁（uninst.exe 不加载 Qt DLL，且删除动作在它退出后才发生）。
void UninstallerWindow::uninstallSelf() {
    // 1) 优先使用与本程序同目录的 uninst.exe（安装版与便携版均随附）。
    QString appDir = QCoreApplication::applicationDirPath();
    QString uninstPath = QDir::toNativeSeparators(appDir + "/uninst.exe");

    // 2) 若同目录没有，则从注册表 Uninstall 项读取 UninstallString 定位。
    if (!QFile::exists(uninstPath)) {
        HKEY hk;
        const wchar_t* key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\UninstallerManager";
        if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hk) == ERROR_SUCCESS) {
            wchar_t buf[1024] = {}; DWORD sz = sizeof(buf);
            if (RegQueryValueExW(hk, L"UninstallString", 0, nullptr, (BYTE*)buf, &sz) == ERROR_SUCCESS) {
                QString ustr = QString::fromWCharArray(buf).trimmed();
                ustr = stripQuotes(ustr).trimmed();
                if (!ustr.isEmpty() && QFile::exists(ustr)) {
                    uninstPath = QDir::toNativeSeparators(ustr);
                }
            }
            RegCloseKey(hk);
        }
    }

    // 3) 两种途径都没有 uninst.exe：说明是未打包的调试/便携运行，提示用户手动删除目录。
    if (!QFile::exists(uninstPath)) {
        QMessageBox::information(this, QString::fromUtf8(u8"卸载本程序"),
            QString::fromUtf8(u8"未找到卸载程序（uninst.exe）。\n\n"
                "如果你是从源码目录或便携包直接运行，直接删除整个程序文件夹即可：\n%1").arg(appDir));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QString::fromUtf8(u8"卸载本程序"));
    box.setText(QString::fromUtf8(u8"确定要卸载「卸载管理器」吗？"));
    box.setInformativeText(QString::fromUtf8(u8"此操作将：\n"
        "• 删除程序文件（%1）\n"
        "• 删除开始菜单快捷方式\n"
        "• 从“设置 ▸ 应用”中移除\n\n"
        "卸载完成后本程序会退出，操作不可撤销。").arg(appDir));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes) return;

    // 4) 启动独立的卸载桩；它会在自身退出后由 PowerShell 删除整个安装目录。
    //    本程序随后立即退出，释放占用的 Qt DLL / 文件锁，确保 uninst.exe 能顺利删除。
    if (!QProcess::startDetached(uninstPath)) {
        QMessageBox::critical(this, QString::fromUtf8(u8"卸载失败"),
            QString::fromUtf8(u8"无法启动卸载程序：\n%1").arg(uninstPath));
        return;
    }
    QApplication::quit();
}

// 关于对话框：集中展示应用名称与版本号（版本号来自 version.h.hpp 的 appVersionFull()）。
void UninstallerWindow::showAbout() {
    QMessageBox::about(this,
        QString::fromUtf8(u8"关于 卸载管理器"),
        QString::fromUtf8(u8"卸载管理器\n版本 %1\n\n"
            "一款用于查看、卸载与清理 Windows 已安装软件及残留项的工具。\n"
            "基于 Qt 6 与 C++ 构建。").arg(appVersionFull()));
}

// 启动时的更新日志弹窗：一打开主界面就展示当前版本的更新内容。
// 用户勾选“不再提示此版本”后，该版本不再弹出（基于 QSettings 持久化到注册表）。
void UninstallerWindow::showUpdatePopup() {
    QSettings settings;
    const QString dontShowKey = QStringLiteral("updatePopup/lastShown");
    QString last = settings.value(dontShowKey).toString();
    // 若当前版本已被用户选择“不再提示”，则不弹。
    if (last == appVersionFull()) {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8(u8"卸载管理器 · 更新日志"));
    dlg.setMinimumWidth(480);

    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QLabel* title = new QLabel(QString::fromUtf8(u8"卸载管理器 %1").arg(appVersionFull()), &dlg);
    title->setStyleSheet("font-size:18px; font-weight:bold; color:#e6e9ee;");
    layout->addWidget(title);

    QLabel* sub = new QLabel(QString::fromUtf8(u8"本次更新内容："), &dlg);
    sub->setStyleSheet("color:#9da3ad;");
    layout->addWidget(sub);

    QTextEdit* te = new QTextEdit(&dlg);
    te->setReadOnly(true);
    te->setPlainText(QString::fromUtf8(
        u8"• 修复带空格的卸载路径被错误截断的问题（如把 “C:\\Program Files\\...” 误判为 “C:\\Program”）\n"
        u8"• 修复 .bat/.cmd/.ps1 等非 exe 启动器在含空格时无法正确启动\n"
        u8"• 改进残留项检测：新增运行中进程护栏，避免误删正在使用的软件（如微信）\n"
        u8"• 支持识别仅残留注册表项的“空壳”软件\n"
        u8"• 修复部分软件（如 OICPP IDE）安装大小显示为 0 的问题\n"
        u8"• 修复部分软件（如腾讯QQ）图标显示错误\n"
        u8"• 修复扫描超大目录（如钉钉）时界面卡死（未响应）的问题"
    ));
    te->setFixedHeight(230);
    layout->addWidget(te);

    QCheckBox* cb = new QCheckBox(QString::fromUtf8(u8"不再提示此版本"), &dlg);
    cb->setStyleSheet("color:#c9cdd3;");
    layout->addWidget(cb);

    QPushButton* ok = new QPushButton(QString::fromUtf8(u8"知道了"), &dlg);
    ok->setDefault(true);
    ok->setStyleSheet(
        "QPushButton { background-color:#3a6df0; color:#ffffff; border:none; "
        "border-radius:6px; padding:8px 20px; font-size:13px; }"
        "QPushButton:hover { background-color:#4f7ef5; }");
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(ok);
    layout->addLayout(btnRow);

    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    int rc = dlg.exec();
    if (rc == QDialog::Accepted && cb->isChecked()) {
        settings.setValue(dontShowKey, appVersionFull());
    }
}

void UninstallerWindow::showDetails() {
    showDetailDialog(m_tableWidget->currentRow());
}

// 正经的软件详情对话框：头部图标+名称/版本，表单列出全部信息，
// 底部“功能”区提供 打开文件所在位置 / 卸载 / 扫描残留 / 复制卸载命令。
void UninstallerWindow::showDetailDialog(int row) {
    auto sw = softwareAtRow(row);
    if (!sw) {
        QMessageBox::warning(this, tr("Tip"), tr("Please select an application"));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(G.TRANSLATOR->tr("Software Details"));
    dlg.resize(600, 580);

    QVBoxLayout* root = new QVBoxLayout(&dlg);

    // 头部：图标 + 名称/版本
    QHBoxLayout* header = new QHBoxLayout();
    QLabel* iconLabel = new QLabel();
    QPixmap pm = iconForSoftware(sw).pixmap(48, 48);
    if (pm.isNull()) pm = QApplication::style()->standardIcon(QStyle::SP_FileIcon).pixmap(48, 48);
    iconLabel->setPixmap(pm);
    QVBoxLayout* titleBox = new QVBoxLayout();
    QLabel* nameLbl = new QLabel(QString::fromStdString(sw->displayName));
    nameLbl->setStyleSheet("font-size:16px; font-weight:bold;");
    QLabel* verLbl = new QLabel(G.TRANSLATOR->tr("Version") + ": " + QString::fromStdString(sw->displayVersion));
    verLbl->setStyleSheet("color:#9da3ad;");
    titleBox->addWidget(nameLbl);
    titleBox->addWidget(verLbl);
    header->addWidget(iconLabel);
    header->addLayout(titleBox);
    header->addStretch();
    root->addLayout(header);

    // 信息表单
    auto roLine = [](const QString& v) {
        QLineEdit* le = new QLineEdit(v);
        le->setReadOnly(true);
        return le;
    };

    QFormLayout* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(G.TRANSLATOR->tr("Scan:"), new QLabel(QString::fromStdString(sw->displayName)));
    form->addRow(G.TRANSLATOR->tr("Version"), new QLabel(QString::fromStdString(sw->displayVersion)));
    form->addRow(G.TRANSLATOR->tr("Publisher"), new QLabel(QString::fromStdString(sw->publisher)));
    form->addRow(G.TRANSLATOR->tr("Install time"), new QLabel(QString::fromStdString(sw->installDate)));
    form->addRow(G.TRANSLATOR->tr("Size"), new QLabel(QString::fromStdString(sw->size.get())));

    QString typeStr = sw->isWindowsInstaller ? "Windows Installer (MSI)"
                     : sw->isSystemComponent ? "系统组件" : "普通程序";
    form->addRow("类型", new QLabel(typeStr));
    QString statusStr = sw->isOrphaned ? QString::fromUtf8(u8"残留（卸载程序不存在）") : QString::fromUtf8(u8"正常");
    form->addRow(QString::fromUtf8(u8"状态"), new QLabel(statusStr));
    form->addRow(G.TRANSLATOR->tr("Install place"), roLine(stripQuotes(QString::fromStdString(sw->installLocation))));
    form->addRow("注册表位置", roLine(QString::fromStdString(sw->orgPath)));

    QTextEdit* uninstallEdit = new QTextEdit(stripCommandQuotes(QString::fromStdString(sw->uninstallString)));
    uninstallEdit->setReadOnly(true);
    uninstallEdit->setMaximumHeight(64);
    form->addRow("卸载命令", uninstallEdit);

    if (!sw->helpLink.empty())
        form->addRow(G.TRANSLATOR->tr("Help link: "), roLine(QString::fromStdString(sw->helpLink)));
    if (!sw->urlInfoAbout.empty())
        form->addRow(G.TRANSLATOR->tr("Infor web"), roLine(QString::fromStdString(sw->urlInfoAbout)));

    root->addLayout(form);

    // 功能按钮区
    QLabel* funcTitle = new QLabel("功能");
    funcTitle->setStyleSheet("font-weight:bold;");
    root->addWidget(funcTitle);

    QGridLayout* funcGrid = new QGridLayout();
    QPushButton* btnOpen = new QPushButton(G.TRANSLATOR->tr("Open file location"));
    QPushButton* btnUninstall = new QPushButton(G.TRANSLATOR->tr("Uninstall selected software"));
    btnUninstall->setObjectName("uninstallBtn");
    QPushButton* btnScan = new QPushButton(G.TRANSLATOR->tr("Scan files"));
    QPushButton* btnCopy = new QPushButton(G.TRANSLATOR->tr("Export software list"));
    funcGrid->addWidget(btnOpen, 0, 0);
    funcGrid->addWidget(btnUninstall, 0, 1);
    funcGrid->addWidget(btnScan, 1, 0);
    funcGrid->addWidget(btnCopy, 1, 1);
    QPushButton* btnDelReg = new QPushButton(QString::fromUtf8(u8"删除残留注册表项"));
    btnDelReg->setObjectName("uninstallBtn");
    funcGrid->addWidget(btnDelReg, 2, 0, 1, 2);
    QPushButton* btnForceDel = new QPushButton(QString::fromUtf8(u8"强制删除此条目"));
    btnForceDel->setObjectName("uninstallBtn");
    funcGrid->addWidget(btnForceDel, 3, 0, 1, 2);
    root->addLayout(funcGrid);

    // 关闭
    QHBoxLayout* closeBox = new QHBoxLayout();
    closeBox->addStretch();
    QPushButton* btnClose = new QPushButton(G.TRANSLATOR->tr("Clean"));
    closeBox->addWidget(btnClose);
    root->addLayout(closeBox);

    connect(btnOpen, &QPushButton::clicked, this, [this, row]() {
        m_tableWidget->setCurrentCell(row, 0);
        openFileLocation();
    });
    connect(btnUninstall, &QPushButton::clicked, &dlg, [this, &dlg, row]() {
        dlg.accept();
        m_tableWidget->setCurrentCell(row, 0);
        uninstallSelected();
    });
    connect(btnScan, &QPushButton::clicked, &dlg, [this, &dlg, row]() {
        dlg.accept();
        m_tableWidget->setCurrentCell(row, 0);
        scanResiduals();
    });
    connect(btnCopy, &QPushButton::clicked, &dlg, [this, &dlg, row]() {
        dlg.accept();
        m_tableWidget->setCurrentCell(row, 0);
        copyUninstallCommand();
    });
    connect(btnDelReg, &QPushButton::clicked, &dlg, [this, &dlg, row]() {
        dlg.accept();
        m_tableWidget->setCurrentCell(row, 0);
        deleteRegistryEntry();
    });
    connect(btnForceDel, &QPushButton::clicked, &dlg, [this, &dlg, row]() {
        dlg.accept();
        m_tableWidget->setCurrentCell(row, 0);
        forceDeleteEntry();
    });
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
}

// 前向声明：openFileLocation 会用到，定义在文件下方（静态辅助函数）。
static QString extractExePath(const QString& cmd);

void UninstallerWindow::openFileLocation() {
    int row = m_tableWidget->currentRow();
    if (tick(row)) return;
    auto sw = softwareAtRow(row);

    QString target;
    bool selectFile = false; // true: explorer /select,<file> 选中具体文件；false: 打开文件夹

    // 优先从卸载命令定位到具体的可执行文件（如 FeverGamesLauncher.exe），
    // 这样“打开文件所在位置”会真正选中该 exe，而不是只打开安装目录。
    QString cmd = QString::fromStdString(Registry::getUninstallCommand(*sw));
    QString exe = extractExePath(cmd);
    if (!exe.isEmpty() && exe.contains("\\") &&
        !exe.contains("msiexec", Qt::CaseInsensitive) &&
        QFile::exists(exe)) {
        target = exe;
        selectFile = true;
    }

    // 若无法定位到 exe 文件，再回退到安装目录
    if (target.isEmpty()) {
        QString installLoc = stripQuotes(QString::fromStdString(sw->installLocation)).trimmed();
        if (!installLoc.isEmpty() && QDir(installLoc).exists()) {
            target = installLoc;
            selectFile = false;
        }
    }

    // 安装目录也没有：再退而求其次，打开卸载命令中 exe 所在的文件夹
    if (target.isEmpty() && !exe.isEmpty() && exe.contains("\\") &&
        !exe.contains("msiexec", Qt::CaseInsensitive)) {
        QFileInfo fi(exe);
        if (fi.isAbsolute() && !fi.absolutePath().isEmpty()) {
            target = fi.absolutePath();
            selectFile = false;
        }
    }

    if (target.isEmpty()) {
        QMessageBox::warning(this, tr("Fail"), tr("Cannot open file location"));
        return;
    }

    // 调用资源管理器：文件用 /select 高亮，文件夹直接打开。
    // 注意：不要把路径手动加引号再用 QStringList 传参——Qt 的 QProcess 在 Windows 上
    // 会对含引号的参数做二次转义，导致 explorer 收到的参数被多转义、选中失败。
    // 正确做法：让 Qt 根据路径是否含空格自行决定是否加引号（即只传 /select,<path>）。
    QString native = QDir::toNativeSeparators(target);
    QStringList args;
    if (selectFile) {
        args << (QString("/select,") + native);
    } else {
        args << native;
    }
    bool ok = QProcess::startDetached("explorer.exe", args);
    if (!ok) {
        QMessageBox::warning(this, tr("Fail"), tr("Cannot open file location"));
    }
}

void UninstallerWindow::toggleShowSystemComponents() {
    m_showSystemComponents = !m_showSystemComponents;
    if (m_showSystemAction) {
        m_showSystemAction->setChecked(m_showSystemComponents);
    }
    updateFindList();
    loadSoftwareList();
}

void UninstallerWindow::exportSoftwareList() {
    QString defaultName = "software_list.txt";
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Show system components"),
        defaultName,
        "Text Files (*.txt);;CSV Files (*.csv);;All Files (*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export software list"), QString::fromUtf8(u8"无法导出文件，请检查保存路径与写入权限。"));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "Name\tVersion\tPublisher\tInstallDate\tSize\tLocation\tStatus\n";
    for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
        if (m_tableWidget->isRowHidden(i)) continue;
        auto sw = softwareAtRow(i);
        if (!sw) continue;
        out << QString::fromStdString(sw->displayName) << "\t"
            << QString::fromStdString(sw->displayVersion) << "\t"
            << QString::fromStdString(sw->publisher) << "\t"
            << QString::fromStdString(sw->installDate) << "\t"
            << QString::fromStdString(sw->size.get()) << "\t"
            << QString::fromStdString(sw->installLocation) << "\t"
            << (sw->isOrphaned ? QString::fromUtf8(u8"残留") : QString::fromUtf8(u8"正常")) << "\n";
    }
    file.close();
    QMessageBox::information(this, tr("Saved"), tr("Software list saved to:\n%1").arg(fileName));
}

void UninstallerWindow::copyUninstallCommand() {
    int row = m_tableWidget->currentRow();
    if (tick(row)) return;

    auto sw = softwareAtRow(row);
    QString cmd = QString::fromStdString(sw->uninstallString);
    if (cmd.isEmpty()) {
        cmd = tr("No command");
    }

    QApplication::clipboard()->setText(cmd);
    QMessageBox::information(this, tr("Copied"), tr("Uninstall command copied to clipboard"));
}

void UninstallerWindow::showDevInfo() {
    sorting();
    int counts[5] = { 0, 0, 0, 0, 0 };
    for (const auto& pair : m_swlist) {
        counts[pair.first] = static_cast<int>(pair.second.size());
    }

    QString info = tr("Qt version: %1\nCompiler: %2\nBuild type: %3\nTotal software: %4\nNormal: %5 | WindowsInstaller: %6 | SystemComponent: %7 | Running: %8 | Unknown: %9")
        .arg(qVersion())
        .arg("Clang 17.0.6 (llvm-mingw1706)")
        .arg("Release")
        .arg(m_softwareList.size())
        .arg(counts[0])
        .arg(counts[1])
        .arg(counts[2])
        .arg(counts[3])
        .arg(counts[4]);

    QMessageBox::information(this, tr("Debug Info"), info);
}

// 把搜索文本统一小写，并把希腊字母 μ 替换成拉丁字母 u，
// 这样用户输入 "uvision" 也能匹配注册表里的 "μVision"。
// 从卸载命令中提取可执行文件路径（去掉引号、取第一个空白前的内容）。
// 例如 "C:\app\uninst.exe" /S -> C:\app\uninst.exe
static QString extractExePath(const QString& cmd) {
    if (cmd.isEmpty()) return QString();
    QString s = cmd.trimmed();
    // 带引号的命令："C:\Program Files\App\uninst.exe" /S
    if (s.startsWith('"')) {
        int end = s.indexOf('"', 1);
        if (end != -1) return s.mid(1, end - 1);
        return s.mid(1);
    }
    // 无引号命令：C:\Program Files\App\uninst.exe /S
    // 必须按 .exe 截取，而不是按第一个空格截取，否则带空格路径会被截断。
    int exePos = s.indexOf(".exe", 0, Qt::CaseInsensitive);
    if (exePos != -1) {
        int end = exePos + 4; // 包含 ".exe"
        int sp = s.indexOf(' ', end);
        return (sp == -1) ? s : s.left(sp);
    }
    // 没有 .exe（如 msiexec 或错误命令）
    int sp = s.indexOf(' ');
    return (sp == -1) ? s : s.left(sp);
}

static QString normalizeForSearch(const QString& s) {
    QString r = s.toLower();
    r.replace(QChar(0x03BC), 'u');   // 希腊小写字母 mu (μ)
    r.replace(QChar(0x00B5), 'u');   // 微符号 (µ)
    return r;
}

void UninstallerWindow::filterSoftware() {
    QString filter = normalizeForSearch(m_searchEdit->text().trimmed());

    // setRowHidden() 在排序启用时与排序代理的 visual/logical 行映射交互不可靠，
    // 会表现为隐藏/显示错乱、过滤不生效。因此：搜索词非空时彻底关闭排序；
    // 清空搜索词后再恢复排序。这样隐藏状态在稳定环境下工作。
    bool wantSorting = filter.isEmpty();
    if (m_tableWidget->isSortingEnabled() != wantSorting) {
        m_tableWidget->setSortingEnabled(wantSorting);
        if (wantSorting) {
            m_tableWidget->sortItems(0, Qt::AscendingOrder);
        }
    }

    int visibleCount = 0;
    filesize_t visibleSize;

    for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
        auto sw = softwareAtRow(i);
        bool match = false;
        if (sw) {
            // 搜索仅在软件名称（displayName）中匹配
            QString text = normalizeForSearch(QString::fromStdString(sw->displayName));
            bool nameMatch = text.contains(filter, Qt::CaseInsensitive);
            // “仅显示残留项”过滤：开启时只保留 isOrphaned 的条目
            bool orphanOk = !m_showOrphanOnly || sw->isOrphaned;
            match = nameMatch && orphanOk;
        }
        m_tableWidget->setRowHidden(i, !match);
        if (match) {
            ++visibleCount;
            if (sw) visibleSize += sw->size.size;
        }
    }

    statusBar()->showMessage(G.TRANSLATOR->tr("Found: %1, Total size: %2").arg(visibleCount).arg(QString::fromStdString(visibleSize.get())));
}

void UninstallerWindow::setupUI() {
    // 幂等：避免 run()/fresh() 多次调用时重复创建菜单与表格导致内存泄漏。
    if (m_uiBuilt) return;
    m_uiBuilt = true;

    setStyleSheet(kAppStyleSheet);

    setWindowTitle(G.TRANSLATOR->tr("Uninstaller") + " " + appVersionFull());
    resize(G.WINDOWS_SIZE[0], G.WINDOWS_SIZE[1]);

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // 菜单栏
    bar = menuBar();
    actionMenu = bar->addMenu(G.TRANSLATOR->tr("Option"));
    actionMenu->addAction(G.TRANSLATOR->tr("Uninstall selected software"), this, &UninstallerWindow::uninstallSelected);
    actionMenu->addAction(G.TRANSLATOR->tr("Scan files"), this, &UninstallerWindow::scanResiduals);
    actionMenu->addAction(G.TRANSLATOR->tr("Look detail"), this, &UninstallerWindow::showDetails);

    // “本程序”菜单：提供卸载自身的能力（区别于卸载列表中的其他软件）。
    QMenu* selfMenu = bar->addMenu(QString::fromUtf8(u8"本程序"));
    selfMenu->addAction(QString::fromUtf8(u8"卸载本程序"), this, &UninstallerWindow::uninstallSelf);
    selfMenu->addSeparator();
    selfMenu->addAction(QString::fromUtf8(u8"关于"), this, &UninstallerWindow::showAbout);

    QMenu* devMenu = bar->addMenu(G.TRANSLATOR->tr("Settings"));
    m_showSystemAction = devMenu->addAction(G.TRANSLATOR->tr("Developer"), this, &UninstallerWindow::toggleShowSystemComponents);
    m_showSystemAction->setCheckable(true);
    m_showSystemAction->setChecked(m_showSystemComponents);
    devMenu->addAction(G.TRANSLATOR->tr("Show system components"), this, &UninstallerWindow::exportSoftwareList);
    devMenu->addAction(G.TRANSLATOR->tr("Export software list"), this, &UninstallerWindow::copyUninstallCommand);
    devMenu->addAction(G.TRANSLATOR->tr("Debug information"), this, &UninstallerWindow::showDevInfo);

    // 工具栏
    QHBoxLayout* toolBar = new QHBoxLayout();

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(G.TRANSLATOR->tr("Search software..."));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &UninstallerWindow::filterSoftware);

    // Ctrl+F 快速聚焦搜索框
    QShortcut* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });

    QPushButton* refreshBtn = new QPushButton(G.TRANSLATOR->tr("Refresh"), this);
    refreshBtn->setObjectName("refreshBtn");
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        built_list();       // 重新扫描注册表（反映新装/卸载的软件）
        loadSoftwareList(); // 重新加载并套用当前搜索过滤
    });

    toolBar->addWidget(new QLabel(G.TRANSLATOR->tr("Scan:")));
    toolBar->addWidget(m_searchEdit);
    toolBar->addStretch();
    toolBar->addWidget(refreshBtn);

    // 仅显示残留项过滤
    m_orphanOnlyCheck = new QCheckBox(QString::fromUtf8(u8"仅显示残留项"), this);
    connect(m_orphanOnlyCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_showOrphanOnly = on;
        filterSoftware();
    });
    toolBar->addWidget(m_orphanOnlyCheck);

    mainLayout->addLayout(toolBar);

    // 软件列表
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(7);
    QStringList headers = { tr("Scan:"), tr("Version"), tr("Install time"), tr("Size"), tr("Publisher"), tr("Install place"), QString::fromUtf8(u8"状态") };
    m_tableWidget->setHorizontalHeaderLabels(headers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(true);
    m_tableWidget->verticalHeader()->setDefaultSectionSize(34);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionsClickable(true);
    m_tableWidget->horizontalHeader()->setSortIndicatorShown(true);
    m_tableWidget->setSortingEnabled(true);

    // 右键菜单：卸载 / 扫描残留 / 详情 / 复制命令
    // 用事件过滤器直接在 viewport 上拦截右键，比 contextMenuPolicy 更可靠。
    m_tableWidget->viewport()->installEventFilter(this);

    mainLayout->addWidget(m_tableWidget);

    // 按钮栏
    QHBoxLayout* buttonBar = new QHBoxLayout();

    QPushButton* uninstallBtn = new QPushButton(G.TRANSLATOR->tr("Uninstall selected software"), this);
    uninstallBtn->setObjectName("uninstallBtn");
    connect(uninstallBtn, &QPushButton::clicked, this, &UninstallerWindow::uninstallSelected);

    QPushButton* scanBtn = new QPushButton(G.TRANSLATOR->tr("Scan files"), this);
    scanBtn->setObjectName("scanBtn");
    connect(scanBtn, &QPushButton::clicked, this, &UninstallerWindow::scanResiduals);

    QPushButton* detailsBtn = new QPushButton(G.TRANSLATOR->tr("Look detail"), this);
    detailsBtn->setObjectName("detailsBtn");
    connect(detailsBtn, &QPushButton::clicked, this, &UninstallerWindow::showDetails);

    buttonBar->addWidget(uninstallBtn);
    buttonBar->addWidget(scanBtn);
    buttonBar->addWidget(detailsBtn);
    buttonBar->addStretch();

    mainLayout->addLayout(buttonBar);
}
