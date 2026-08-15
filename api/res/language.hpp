/*
* The language file included in the release.
*/
#pragma once
#include "qt.hpp"
#include "std.hpp"
#include "global.hpp"

// 定义一个轻量级的结构体
struct LangEntry {
	std::array<QStringView, LANGTYPE> string;
};

constexpr QStringView NONETEXT {u""};

// 使用 constexpr 数组存储语言数据
constexpr std::array<LangEntry, LANGSIZE> LANG_MAP {{
	{u"Option", u"操作"},
	{u"Exit", u"退出"},
	{u"Scanning software ...", u"正在扫描已安装软件..."},
	{u"Clean", u"取消"},
	{u"Loading: %1, %2/%3", u"加载：%1, %2/%3"},
	{u"Loading: %1, %2/%3 - found: %4", u"加载：%1, %2/%3 - 已找到：%4"},
	{u"Reload ...", u"重载 ..."},
	{u"Found: %1, Total size: %2", u"共找到 %1 个软件，共占 %2"},
	{u"Tip", u"提示"},
	{u"Please select an application", u"请先选择一个软件"},
	{u"Ensure uninstaller", u"确认卸载"},
	{u"Sure to uninstaller the following software?\n\nSoftware: %1 Version: %2\n\nNote: It may take a while to uninstall, please wait patiently.", u"确定要卸载以下软件吗？\n\n软件: %1\n版本: %2\n\n注意：卸载过程可能需要几分钟，请耐心等待。"},
	{u"Uninstalling %1 ...", u"正在卸载 %1 ..."},
	{u"Successful", u"成功"},
	{u"Software uninstalled successfully.\n\nScan residual files?", u"软件已成功卸载！\n\n是否扫描残留文件？"},
	{u"Fail", u"失败"},
	{u"Uninstall fail", u"卸载失败，请手动卸载。"},
	{u"Scanning files ...", u"正在扫描残留文件..."},
	{u"Scan result", u"扫描结果"},
	{u"Not found", u"未发现残留文件。"},
	{u"File list", u"残留文件列表"},
	{u"Uninstaller", u"软件卸载管理器"},
	{u"Software: %1\n\n", u"软件: %1\n\n"},
	{u"Found %1 files: \n\n", u"找到 %1 个残留项:\n\n"},
	{u"Delete all", u"删除所有残留"},
	{u"Deletion completed!", u"残留文件已删除！"},
	{u"Some delete fail.", u"部分文件删除失败。"},
	{u"Software name: %1\nVersion: %2\nPublisher: %3\nInstall date: %4\nInstall location: %5\n Registry place: %6\nUninstall command: %7\nSize: %8\n", u"软件名称: %1\n版本: %2\n发行商: %3\n安装日期: %4\n安装位置: %5\n注册表位置: %6\n卸载命令: %7\n估计大小: %8\n"},
	{u"Help link: ", u"帮助链接: "},
	{u"Infor web", u"信息网址: "},
	{u"Detail", u"详细信息"},
	{u"Uninstall selected software", u"卸载选中软件"},
	{u"Scan files", u"扫描残留文件"},
	{u"Look detail", u"查看详细信息"},
	{u"Search software...", u"搜索软件..."},
	{u"Refresh", u"刷新列表"},
	{u"Scan:", u"搜索:"},
	{u"Software name", u"软件名称"},
	{u"Version", u"版本"},
	{u"Install time", u"安装日期"},
	{u"Size", u"大小"},
	{u"Publisher", u"发行商"},
	{u"Install place", u"安装位置"},
	{u"Settings", u"设置"},
	{u"Developer", u"开发者"},
	{u"Show system components", u"显示系统组件"},
	{u"Export software list", u"导出软件列表"},
	{u"Copy uninstall command", u"复制卸载命令"},
	{u"Debug information", u"调试信息"},
	{u"Saved", u"已保存"},
	{u"Software list saved to:\n%1", u"软件列表已保存到:\n%1"},
	{u"Copied", u"已复制"},
	{u"Uninstall command copied to clipboard", u"卸载命令已复制到剪贴板"},
	{u"Debug Info", u"调试信息"},
	{u"Qt version: %1\nCompiler: %2\nBuild type: %3\nTotal software: %4\nNormal: %5 | WindowsInstaller: %6 | SystemComponent: %7 | Running: %8 | Unknown: %9", u"Qt 版本: %1\n编译器: %2\n构建类型: %3\n软件总数: %4\n普通: %5 | WindowsInstaller: %6 | 系统组件: %7 | 运行中: %8 | 未知: %9"},
	{u"No command", u"无卸载命令"},
	{u"Command to execute:", u"即将执行的卸载命令:"},
	{u"Open file location", u"打开文件所在位置"},
	{u"Install location empty, opened uninstaller folder", u"安装位置为空，已打开卸载程序所在文件夹"},
	{u"Cannot open file location", u"无法打开文件位置"},
	{u"Software Details", u"软件详情"},
	{u"", u""},
	{u"", u""}
}};

// 提供一个 language 查找函数
// id: 0x0~0xffffffffu-1 type: uint-type
const QStringView& getlang(uint id, uint type = 0xffffffffu);