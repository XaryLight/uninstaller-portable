/*
* Mainwindow include file
*/

#ifndef UNINSTALLER_MAINWINDOW_H
#define UNINSTALLER_MAINWINDOW_H
#include <reg/registry.h>
#include <res/global.h>
#include <base/std.h>
#include <base/qt.h>

using namespace std;

class UninstallerWindow : public QMainWindow // Should be QMainWindow
{
    Q_OBJECT

public:
    void run();
    void fresh();
    explicit UninstallerWindow(QWidget* parent = nullptr);

private slots:
    bool tick(const ll& row);                   //选择检查
    void sorting();                             //排序
    void built_list();                          //获取软件信息
    void showDetails();                         //显示详细信息
    void openFileLocation();                    //打开文件所在位置（详情/右键菜单）
    void scanResiduals();                       //扫描残留
    void filterSoftware();
    void loadSoftwareList(); //加载
    void uninstallSelected();
    void toggleShowSystemComponents();          //开发者：切换显示系统组件
    void deleteRegistryEntry();                 //删除残留注册表项（清理僵尸条目）
    void forceDeleteEntry();                    //强制删除此条目（绕过残留检测，删除注册表项与磁盘残留）
    void uninstallSelf();                       //卸载本程序（自卸载，由独立 uninst.exe 负责删目录）
    void showAbout();                           //关于本程序（展示版本号）
    void exportSoftwareList();                  //开发者：导出软件列表
    void copyUninstallCommand();                //开发者：复制卸载命令
    void showDevInfo();                         //开发者：显示调试信息

private:
    void setupUI();
    void updateFindList();
    SoftwareInfo* softwareAtRow(int row) const; // 通过表格行的 UserRole 取 SoftwareInfo*
    bool eventFilter(QObject* obj, QEvent* event) override; // 拦截表格右键
    void onTableContextMenu(const QPoint& pos); // 表格右键菜单
    void showDetailDialog(int row);             // 软件详情对话框（列出信息 + 功能按钮）
    void showUpdatePopup();                     // 启动时的更新日志弹窗（一打开主界面即弹出）

    // Members
    int len{ 0 };
    bool m_uiBuilt{ false };
    bool m_busy{ false };   // 防止卸载/扫描过程中重复触发
    bool m_showSystemComponents{ false }; // 默认隐藏系统组件，减少 VC++ 运行库等视觉噪音
    bool m_showOrphanOnly{ false };        // 仅显示残留项
    QMenu* actionMenu{ nullptr };
    QAction* m_showSystemAction{ nullptr };
    QMenuBar* bar{ nullptr };
    QCheckBox* m_orphanOnlyCheck{ nullptr };
    filesize_t total_size;
    QLineEdit* m_searchEdit{ nullptr };
    QTableWidget* m_tableWidget{ nullptr };
    vector<int> findlist{ 0, 1 };
    vector<SoftwareInfo> m_softwareList;
    map<int, vector<SoftwareInfo*>> m_swlist;
};

#endif //UNINSTALLER_MAINWINDOW_H
