#include <qgsapplication.h>
#include <qgsproviderregistry.h>
#include "mainwindow.h"
#include <QDebug>

int main(int argc, char *argv[]) {
    // 1. 优先设置 Linux 环境变量
    qputenv("QGIS_PREFIX_PATH", "/usr");
    qputenv("PROJ_LIB", "/usr/share/proj");

    // 2. 实例化 QgsApplication，必须在一切 GUI 和 QGIS 对象之前
    QgsApplication app(argc, argv, true); 

    QString prefixPath = "/usr";
    QString pluginPath = "/usr/lib/qgis/plugins";
    
    QgsApplication::setPrefixPath(prefixPath, true);
    QgsApplication::setPluginPath(pluginPath);
    
    // 3. 核心初始化
    QgsApplication::initQgis();
    QgsProviderRegistry::instance(pluginPath);

    qDebug() << "========================================";
    qDebug() << "✅ QGIS 框架初始化成功！准备拉起界面...";
    qDebug() << "========================================";

    // 4. 延迟安全创建主界面
    MainWindow* mainWindow = new MainWindow();
    mainWindow->show();

    int execCode = app.exec();
    
    // 5. 安全卸载资源
    delete mainWindow;
    QgsApplication::exitQgis();
    return execCode;
}