/*
* The project *Unsinstaller* which aim to uninstall the software cleanly.
*/

#include "mainwindow.hpp"
#include "version.hpp"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	// 加载 exe 内嵌的程序图标（IDI_APP_ICON），作为窗口标题栏与任务栏图标
	app.setWindowIcon(QIcon(":/appicon.ico"));
	// 设置应用名称与版本，便于系统/任务管理器识别（“像正常软件一样”）。
	app.setApplicationName(QString::fromUtf8(u8"卸载管理器"));
	app.setApplicationDisplayName(QString::fromUtf8(u8"卸载管理器"));
	app.setApplicationVersion(QString::fromUtf8(APP_VERSION));

	UninstallerWindow window;
	window.run();

	return app.exec();
}
