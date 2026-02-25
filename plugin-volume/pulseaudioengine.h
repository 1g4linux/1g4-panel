/* plugin-volume/pulseaudioengine.h
 * Volume control plugin implementation
 */

#ifndef PULSEAUDIOENGINE_H
#define PULSEAUDIOENGINE_H

#include "audioengine.h"

#include <QObject>
#include <QList>
#include <QTimer>
#include <QMap>

#include <atomic>

#include <pulse/pulseaudio.h>

// PA_VOLUME_UI_MAX is only supported since pulseaudio 0.9.23
#ifndef PA_VOLUME_UI_MAX
#define PA_VOLUME_UI_MAX (pa_sw_volume_from_dB(+11.0))
#endif

class AudioDevice;

class PulseAudioEngine : public AudioEngine {
  Q_OBJECT

 public:
  PulseAudioEngine(QObject* parent = nullptr);
  ~PulseAudioEngine() override;

  virtual const QString backendName() const override { return QLatin1String("PulseAudio"); }

  int volumeMax(AudioDevice* /*device*/) const override { return m_maximumVolume; }
  bool isShuttingDown() const { return m_shuttingDown.load(std::memory_order_acquire); }

  void requestSinkInfoUpdate(uint32_t idx);
  void removeSink(uint32_t idx);
  void addOrUpdateSink(const pa_sink_info* info);
  void addOrUpdateSinkSnapshot(const QString& name,
                               uint32_t index,
                               const QString& description,
                               bool mute,
                               pa_cvolume cvolume);

  pa_context_state_t contextState() const { return m_contextState; }
  bool ready() const { return m_ready; }
  pa_threaded_mainloop* mainloop() const { return m_mainLoop; }

 public slots:
  void commitDeviceVolume(AudioDevice* device);
  void retrieveSinkInfo(uint32_t idx);
  void setMute(AudioDevice* device, bool state);
  void setContextState(pa_context_state_t state);
  void setIgnoreMaxVolume(bool ignore);

 signals:
  void sinkInfoChanged(uint32_t idx);
  void contextStateChanged(pa_context_state_t state);
  void readyChanged(bool ready);

 private slots:
  void handleContextStateChanged();
  void connectContext();

 private:
  void shutdownContext();
  void retrieveSinks();
  void setupSubscription();

  pa_mainloop_api* m_mainLoopApi;
  pa_threaded_mainloop* m_mainLoop;
  pa_context* m_context;

  pa_context_state_t m_contextState;
  bool m_ready;
  QTimer m_reconnectionTimer;
  int m_maximumVolume;
  std::atomic_bool m_shuttingDown;

  QMap<AudioDevice*, pa_cvolume> m_cVolumeMap;
};

#endif  // PULSEAUDIOENGINE_H
