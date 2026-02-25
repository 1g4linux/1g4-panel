/* plugin-volume/oneg4volume.h
 * Volume control plugin implementation
 */

#ifndef ONEG4VOLUME_H
#define ONEG4VOLUME_H

#include "../panel/ioneg4panelplugin.h"
#include <QList>
#include <QToolButton>
#include <QSlider>
#include <QPointer>

#include <optional>

class VolumeButton;
class AudioEngine;
class AudioDevice;
class QDialog;
namespace OneG4 {
class Notification;
}

class OneG4VolumeConfiguration;

class OneG4Volume : public QObject, public IOneG4PanelPlugin {
  Q_OBJECT
 public:
  OneG4Volume(const IOneG4PanelPluginStartupInfo& startupInfo);
  ~OneG4Volume();

  virtual QWidget* widget();
  virtual QString themeId() const { return QStringLiteral("Volume"); }
  virtual IOneG4PanelPlugin::Flags flags() const { return PreferRightAlignment | HaveConfigDialog; }
  void realign();
  QDialog* configureDialog();

  void setAudioEngine(AudioEngine* engine);
 protected slots:
  virtual void settingsChanged();
  void handleSinkListChanged();
  void handleEngineStateChanged();
  void showNotification() const;
  void openExternalMixer();

 private:
  void setDefaultSink(AudioDevice* sink);
  std::optional<uint> observedDefaultSinkId() const;
  AudioDevice* findSinkById(const QList<AudioDevice*>& sinks, uint sinkId) const;

  AudioEngine* m_engine;
  VolumeButton* m_volumeButton;
  uint m_defaultSinkId;
  QPointer<AudioDevice> m_defaultSink;
  OneG4::Notification* m_notification;
  QPointer<OneG4VolumeConfiguration> m_configDialog;
  QPointer<QDialog> m_mixerDialog;
  bool m_alwaysShowNotifications;
};

class OneG4VolumePluginLibrary : public QObject, public IOneG4PanelPluginLibrary {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "oneg4.org/Panel/PluginInterface/3.0")
  Q_INTERFACES(IOneG4PanelPluginLibrary)
 public:
  IOneG4PanelPlugin* instance(const IOneG4PanelPluginStartupInfo& startupInfo) const {
    return new OneG4Volume(startupInfo);
  }
};

#endif  // ONEG4VOLUME_H
