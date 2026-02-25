/* plugin-volume/testaudioengine.h
 * Development-only in-process backend used for UI and plugin tests.
 */

#ifndef TESTAUDIOENGINE_H
#define TESTAUDIOENGINE_H

#include "audioengine.h"

class TestAudioEngine : public AudioEngine {
  Q_OBJECT

 public:
  explicit TestAudioEngine(QObject* parent = nullptr);
  ~TestAudioEngine() override = default;

  const QString backendName() const override { return QLatin1String("TestBackend"); }
  int volumeMax(AudioDevice* device) const override;

 public slots:
  bool commitDeviceVolume(AudioDevice* device) override;
  bool setMute(AudioDevice* device, bool state) override;
};

#endif  // TESTAUDIOENGINE_H
