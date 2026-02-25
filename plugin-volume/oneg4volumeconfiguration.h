/* plugin-volume/oneg4volumeconfiguration.h
 * Volume control plugin implementation
 */

#ifndef ONEG4VOLUMECONFIGURATION_H
#define ONEG4VOLUMECONFIGURATION_H

#include "../panel/oneg4panelpluginconfigdialog.h"
#include "../panel/pluginsettings.h"

#include <QList>
#include <QPointer>
#include <memory>

#define SETTINGS_MUTE_ON_MIDDLECLICK "showOnMiddleClick"
#define SETTINGS_DEVICE "device"
#define SETTINGS_DEVICE_ID_MIGRATION_DONE "deviceIdMigrationDone"
#define SETTINGS_STEP "volumeAdjustStep"
#define SETTINGS_IGNORE_MAX_VOLUME "ignoreMaxVolume"
#define SETTINGS_AUDIO_ENGINE "audioEngine"
#define SETTINGS_ALWAYS_SHOW_NOTIFICATIONS "alwaysShowNotifications"

#define SETTINGS_DEFAULT_MUTE_ON_MIDDLECLICK true
#define SETTINGS_DEFAULT_DEVICE 0
#define SETTINGS_DEFAULT_STEP 3
#define SETTINGS_DEFAULT_AUDIO_ENGINE "PipeWire"
#define SETTINGS_DEFAULT_IGNORE_MAX_VOLUME true
#define SETTINGS_DEFAULT_ALWAYS_SHOW_NOTIFICATIONS false

class AudioDevice;
class QShowEvent;
class QTreeWidgetItem;
class WirePlumberPolicy;

namespace Ui {
class OneG4VolumeConfiguration;
}

class OneG4VolumeConfiguration : public OneG4PanelPluginConfigDialog {
  Q_OBJECT

 public:
  explicit OneG4VolumeConfiguration(PluginSettings* settings, bool ossAvailable, QWidget* parent = nullptr);
  ~OneG4VolumeConfiguration();

 public slots:
  void setSinkList(const QList<AudioDevice*> sinks);
  void sinkSelectionChanged(int index);
  void muteOnMiddleClickChanged(bool state);
  void stepSpinBoxChanged(int step);
  void ignoreMaxVolumeCheckBoxChanged(bool state);
  void alwaysShowNotificationsCheckBoxChanged(bool state);
  void audioBackendChanged(int index);
  void policyItemChanged(QTreeWidgetItem* item, int column);
  void applyPolicy();

 protected:
  void showEvent(QShowEvent* event) override;

 protected slots:
  virtual void loadSettings();

 private:
  Ui::OneG4VolumeConfiguration* ui;
  bool mLockSettingChanges;
  bool mUpdatingPolicyTree;
  bool mPolicyDirty;
  bool mHasDeferredSinkList;
  QList<QPointer<AudioDevice>> mDeferredSinkList;
  std::unique_ptr<WirePlumberPolicy> m_policy;

  void applySinkListToUi(const QList<AudioDevice*>& sinks);
  void rebuildPolicyTree(const QList<AudioDevice*>& sinks);
  static QString deriveCardName(const QString& deviceName);
  void setPolicyDirty(bool dirty);
};

#endif  // ONEG4VOLUMECONFIGURATION_H
