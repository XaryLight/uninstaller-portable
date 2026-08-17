/*
* The project *Unsinstaller* which aim to uninstall the software cleanly.
*/

#include <base/qt.h>
#include <res/version.h>
#include <ui/mainwindow.h>
#include <res/constexpr.h>

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	// 1. Determine the level prefix
	const char *level;
	switch (type) {
		case QtDebugMsg:    level = "DEBUG"; break;
		case QtInfoMsg:     level = "INFO "; break;
		case QtWarningMsg:  level = "WARN "; break;
		case QtCriticalMsg: level = "CRIT "; break;
		case QtFatalMsg:    level = "FATAL"; break;
		default:            level = "UNKNOWN"; break;
	}

	// 2. Print to stderr using fprintf for reliability and ease of use with C-style strings
	// context.file, context.line, and context.function are available in QMessageLogContext
	fprintf(stderr, "[%s] %s:%d (%s) -> %s\n",
			 level,
			 context.file ? context.file : "unknown file",
			 context.line,
			 context.function ? context.function : "unknown func",
			 msg.toUtf8().constData());

	// 3. Flush and handle fatal errors
	fflush(stderr);
	if (type == QtFatalMsg) {
		abort();
	}
}


bool setupTranslations(QApplication &app) {
	G.TRANSLATOR = new QTranslator(&app); // 交 same 作用域给 app 管理内存
	// 1. 获取当前系统的语言标识 (例如 "zh_CN", "en_US")
	QString systemLocale = QLocale::system().name();
	// 2. 定义基础路径 (假设 .qm 文件在可执行文件目录下的 i18n 文件夹)
	// 如果是 Qt Resource (.qrc)，则路径以 ":" 开头
	// 3. 构建目标文件名
	QString targetFile = PATH_LANGBASE + systemLocale + ".qm";
	// 4. 尝试加载系统语言
	if (G.TRANSLATOR->load(targetFile)) {
		app.installTranslator(G.TRANSLATOR);
		qDebug() << "Successfully loaded:" << targetFile;
		return true;
	}
	// 5. 如果失败，尝试加载默认英文
	qDebug() << "Failed to load:" << targetFile << ". Trying default:" << PATH_LANGDEFAULT;
	if (G.TRANSLATOR->load(targetFile)) {
		app.installTranslator(G.TRANSLATOR);
		qDebug() << "Successfully loaded fallback:" << PATH_LANGDEFAULT;
		return true;
	}
	// 6. 如果都失败，打印警告
	qDebug() << "Critical: No translation files found!";
	return false;
}

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	qInstallMessageHandler(myMessageOutput);
	setupTranslations(app);
	// 加载 exe 内嵌的程序图标（IDI_APP_ICON），作为窗口标题栏与任务栏图标
	app.setWindowIcon(QIcon(PATH_ICON));
	// 设置应用名称与版本，便于系统/任务管理器识别（“像正常软件一样”）。
	app.setApplicationName(G.TRANSLATOR->tr("Uninstaller"));
	app.setApplicationDisplayName(G.TRANSLATOR->tr("Uninstaller"));
	app.setApplicationVersion(QString::fromUtf8(APP_VERSION));

	UninstallerWindow window;
	window.run();

	return app.exec();
}
