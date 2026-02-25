#include <QPluginLoader>
#include <QSettings>
#include <QStringList>
#include <QtTest>
#include <QFileInfo>

#include "../panel/ioneg4panelplugin.h"

class VolumePluginLoadTest : public QObject {
  Q_OBJECT

 private slots:
  void exportsPanelPluginInterface();
  void desktopMetadataDeclaresBaselineIcon();
};

void VolumePluginLoadTest::exportsPanelPluginInterface() {
  const QString pluginPath = qEnvironmentVariable("ONEG4_VOLUME_PLUGIN_PATH");
  QVERIFY2(!pluginPath.isEmpty(), "ONEG4_VOLUME_PLUGIN_PATH must be set");
  QVERIFY2(QFileInfo::exists(pluginPath), qPrintable(QStringLiteral("Plugin path does not exist: %1").arg(pluginPath)));

  QPluginLoader loader(pluginPath);
  QVERIFY2(loader.load(), qPrintable(loader.errorString()));

  QObject* instance = loader.instance();
  QVERIFY2(instance != nullptr, qPrintable(loader.errorString()));

  auto* panelPluginLibrary = qobject_cast<IOneG4PanelPluginLibrary*>(instance);
  QVERIFY2(panelPluginLibrary != nullptr, "Loaded module does not export IOneG4PanelPluginLibrary");
}

void VolumePluginLoadTest::desktopMetadataDeclaresBaselineIcon() {
  const QString desktopPath = qEnvironmentVariable("ONEG4_VOLUME_DESKTOP_PATH");
  QVERIFY2(!desktopPath.isEmpty(), "ONEG4_VOLUME_DESKTOP_PATH must be set");
  QVERIFY2(QFileInfo::exists(desktopPath),
           qPrintable(QStringLiteral("Desktop metadata path does not exist: %1").arg(desktopPath)));

  QSettings desktopFile(desktopPath, QSettings::IniFormat);
  QVERIFY2(desktopFile.childGroups().contains(QStringLiteral("Desktop Entry")),
           "Desktop metadata is missing [Desktop Entry]");

  desktopFile.beginGroup(QStringLiteral("Desktop Entry"));

  QCOMPARE(desktopFile.value(QStringLiteral("Type")).toString(), QStringLiteral("Service"));
  QCOMPARE(desktopFile.value(QStringLiteral("Icon")).toString(), QStringLiteral("multimedia-volume-control"));

  const QString serviceTypes = desktopFile.value(QStringLiteral("ServiceTypes")).toString();
  QVERIFY2(serviceTypes.contains(QStringLiteral("OneG4Panel/Plugin")),
           "Desktop metadata does not advertise OneG4 panel plugin service type");
}

QTEST_APPLESS_MAIN(VolumePluginLoadTest)

#include "plugin_volume_plugin_load_test.moc"
