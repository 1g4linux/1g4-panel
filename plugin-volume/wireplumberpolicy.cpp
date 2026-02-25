/* plugin-volume/wireplumberpolicy.cpp
 * Helper for writing WirePlumber SPA-JSON rules and restarting the daemon.
 */

#include "wireplumberpolicy.h"
#include "volumelogging.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QtDebug>
#include <QtGlobal>

#include <algorithm>

namespace {
QString ensureConfigBase() {
  QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  if (base.isEmpty()) {
    base = QDir::homePath() + QLatin1String("/.config");
  }
  return base;
}
}  // namespace

WirePlumberPolicy::WirePlumberPolicy(const QString& configDir, const QString& fileName)
    : m_configDir(configDir),
      m_fileName(fileName),
      m_restartCommand(QStringList{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("restart"),
                                   QStringLiteral("wireplumber")}) {}

QString WirePlumberPolicy::defaultConfigDir() {
  return QDir::cleanPath(ensureConfigBase() + QLatin1String("/wireplumber/wireplumber.conf.d"));
}

QString WirePlumberPolicy::defaultFileName() {
  return QStringLiteral("60-1g4-panel-volume.conf");
}

QString WirePlumberPolicy::filePath() const {
  return QDir(m_configDir).filePath(m_fileName);
}

void WirePlumberPolicy::setDeviceDisabled(const QString& name, bool disabled) {
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty())
    return;

  if (disabled)
    m_disabledDevices.insert(trimmed);
  else
    m_disabledDevices.remove(trimmed);
}

void WirePlumberPolicy::setNodeDisabled(const QString& name, bool disabled) {
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty())
    return;

  if (disabled)
    m_disabledNodes.insert(trimmed);
  else
    m_disabledNodes.remove(trimmed);
}

bool WirePlumberPolicy::deviceDisabled(const QString& name) const {
  return m_disabledDevices.contains(name);
}

bool WirePlumberPolicy::nodeDisabled(const QString& name) const {
  return m_disabledNodes.contains(name);
}

void WirePlumberPolicy::setRestartCommand(const QStringList& command) {
  if (!command.isEmpty())
    m_restartCommand = command;
}

bool WirePlumberPolicy::load() {
  m_disabledDevices.clear();
  m_disabledNodes.clear();

  QFile file(filePath());
  if (!file.exists())
    return true;
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(lcVolumePersistence) << "WirePlumberPolicy: unable to open policy file" << file.fileName();
    return false;
  }

  QJsonParseError error{};
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject())
    return false;

  const QJsonArray rules = doc.object().value(QStringLiteral("monitor.alsa.rules")).toArray();
  for (const QJsonValue& ruleVal : rules) {
    const QJsonObject rule = ruleVal.toObject();
    const QJsonArray matches = rule.value(QStringLiteral("matches")).toArray();
    QStringList deviceNames;
    QStringList nodeNames;
    for (const QJsonValue& matchVal : matches) {
      const QJsonObject matchObj = matchVal.toObject();
      const QString devName = matchObj.value(QStringLiteral("device.name")).toString();
      const QString nodeName = matchObj.value(QStringLiteral("node.name")).toString();
      if (!devName.isEmpty())
        deviceNames << devName;
      if (!nodeName.isEmpty())
        nodeNames << nodeName;
    }

    const QJsonObject actions = rule.value(QStringLiteral("actions")).toObject();
    const QJsonObject props = actions.value(QStringLiteral("update-props")).toObject();
    const bool disableDevice = props.value(QStringLiteral("device.disabled")).toBool(false);
    const bool disableNode = props.value(QStringLiteral("node.disabled")).toBool(false);
    if (disableDevice) {
      for (const QString& devName : std::as_const(deviceNames))
        m_disabledDevices.insert(devName);
    }
    if (disableNode) {
      for (const QString& nodeName : std::as_const(nodeNames))
        m_disabledNodes.insert(nodeName);
    }
  }
  return true;
}

QJsonDocument WirePlumberPolicy::buildDocument() const {
  QJsonArray rules;

  QStringList devices = m_disabledDevices.values();
  QStringList nodes = m_disabledNodes.values();
  std::sort(devices.begin(), devices.end());
  std::sort(nodes.begin(), nodes.end());

  for (const QString& deviceName : devices) {
    QJsonObject rule;
    QJsonArray matches;
    matches.append(QJsonObject{{QStringLiteral("device.name"), deviceName}});
    QJsonObject props{{QStringLiteral("device.disabled"), true}};
    QJsonObject actions{{QStringLiteral("update-props"), props}};
    rule.insert(QStringLiteral("matches"), matches);
    rule.insert(QStringLiteral("actions"), actions);
    rules.append(rule);
  }

  for (const QString& nodeName : nodes) {
    QJsonObject rule;
    QJsonArray matches;
    matches.append(QJsonObject{{QStringLiteral("node.name"), nodeName}});
    QJsonObject props{{QStringLiteral("node.disabled"), true}};
    QJsonObject actions{{QStringLiteral("update-props"), props}};
    rule.insert(QStringLiteral("matches"), matches);
    rule.insert(QStringLiteral("actions"), actions);
    rules.append(rule);
  }

  QJsonObject root;
  root.insert(QStringLiteral("comment"), QStringLiteral("Managed by OneG4 panel volume plugin"));
  if (!rules.isEmpty())
    root.insert(QStringLiteral("monitor.alsa.rules"), rules);

  return QJsonDocument(root);
}

void WirePlumberPolicy::ensureParentDir() const {
  QDir dir;
  dir.mkpath(m_configDir);
}

bool WirePlumberPolicy::write() const {
  ensureParentDir();

  QFile file(filePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qCWarning(lcVolumePersistence) << "WirePlumberPolicy: unable to write policy file" << file.fileName();
    return false;
  }

  const QJsonDocument doc = buildDocument();
  file.write(doc.toJson(QJsonDocument::Indented));
  return true;
}

bool WirePlumberPolicy::restart(QString* errorString) const {
  if (m_restartCommand.isEmpty()) {
    if (errorString)
      *errorString = QStringLiteral("No restart command configured");
    return false;
  }

  QProcess proc;
  proc.start(m_restartCommand.first(), m_restartCommand.mid(1));
  if (!proc.waitForFinished(5000)) {
    if (errorString)
      *errorString = QStringLiteral("Timed out restarting WirePlumber");
    return false;
  }

  if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
    if (errorString) {
      *errorString = QString::fromLocal8Bit(proc.readAllStandardError());
      if (errorString->isEmpty())
        *errorString = QStringLiteral("WirePlumber restart failed with code %1").arg(proc.exitCode());
    }
    return false;
  }

  return true;
}
