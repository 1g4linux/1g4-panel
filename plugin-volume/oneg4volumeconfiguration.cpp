/* plugin-volume/oneg4volumeconfiguration.cpp
 * Volume control plugin implementation
 */

#include "oneg4volumeconfiguration.h"
#include "ui_oneg4volumeconfiguration.h"

#include "audiodevice.h"
#include "sinkselection.h"
#include "wireplumberpolicy.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QtGlobal>

OneG4VolumeConfiguration::OneG4VolumeConfiguration(PluginSettings* settings, bool /*ossAvailable*/, QWidget* parent)
    : OneG4PanelPluginConfigDialog(settings, parent),
      ui(new Ui::OneG4VolumeConfiguration),
      mLockSettingChanges(false),
      mUpdatingPolicyTree(false),
      mPolicyDirty(false),
      mHasDeferredSinkList(false),
#ifdef ONEG4_VOLUME_ENABLE_WIREPLUMBER_POLICY
      m_policy(std::make_unique<WirePlumberPolicy>()) {
#else
      m_policy(nullptr) {
#endif
  ui->setupUi(this);

  if (m_policy)
    m_policy->load();

  loadSettings();

  connect(ui->audioBackendCombo, &QComboBox::currentIndexChanged, this, &OneG4VolumeConfiguration::audioBackendChanged);
  connect(ui->devAddedCombo, &QComboBox::currentIndexChanged, this, &OneG4VolumeConfiguration::sinkSelectionChanged);
  connect(ui->buttons, &QDialogButtonBox::clicked, this, &OneG4VolumeConfiguration::dialogButtonsAction);
  connect(ui->muteOnMiddleClickCheckBox, &QCheckBox::toggled, this,
          &OneG4VolumeConfiguration::muteOnMiddleClickChanged);
  connect(ui->stepSpinBox, &QSpinBox::valueChanged, this, &OneG4VolumeConfiguration::stepSpinBoxChanged);
  connect(ui->ignoreMaxVolumeCheckBox, &QCheckBox::toggled, this,
          &OneG4VolumeConfiguration::ignoreMaxVolumeCheckBoxChanged);
  connect(ui->alwaysShowNotificationsCheckBox, &QAbstractButton::toggled, this,
          &OneG4VolumeConfiguration::alwaysShowNotificationsCheckBoxChanged);

  if (m_policy) {
    connect(ui->policyTree, &QTreeWidget::itemChanged, this, &OneG4VolumeConfiguration::policyItemChanged);
    connect(ui->applyPolicyButton, &QPushButton::clicked, this, &OneG4VolumeConfiguration::applyPolicy);
    ui->policyTree->setRootIsDecorated(false);
    ui->policyTree->setAlternatingRowColors(true);
  }
  else {
    ui->policyGroupBox->hide();
  }

  setPolicyDirty(false);
}

OneG4VolumeConfiguration::~OneG4VolumeConfiguration() {
  delete ui;
}

void OneG4VolumeConfiguration::setSinkList(const QList<AudioDevice*> sinks) {
  if (!isVisible()) {
    mDeferredSinkList.clear();
    mDeferredSinkList.reserve(sinks.size());
    for (AudioDevice* sink : sinks) {
      mDeferredSinkList.append(QPointer<AudioDevice>(sink));
    }
    mHasDeferredSinkList = true;
    return;
  }

  mHasDeferredSinkList = false;
  mDeferredSinkList.clear();
  applySinkListToUi(sinks);
}

void OneG4VolumeConfiguration::showEvent(QShowEvent* event) {
  OneG4PanelPluginConfigDialog::showEvent(event);
  if (!mHasDeferredSinkList) {
    return;
  }

  QList<AudioDevice*> sinks;
  sinks.reserve(mDeferredSinkList.size());
  for (const QPointer<AudioDevice>& sink : std::as_const(mDeferredSinkList)) {
    if (sink) {
      sinks.append(sink.data());
    }
  }

  mHasDeferredSinkList = false;
  mDeferredSinkList.clear();
  applySinkListToUi(sinks);
}

void OneG4VolumeConfiguration::applySinkListToUi(const QList<AudioDevice*>& sinks) {
  const QVariant storedSinkSetting = settings().value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE);

  const bool oldBlock = ui->devAddedCombo->blockSignals(true);
  ui->devAddedCombo->clear();
  QList<uint> sinkIds;
  sinkIds.reserve(sinks.size());

  for (const AudioDevice* dev : std::as_const(sinks)) {
    sinkIds.append(dev->index());
    ui->devAddedCombo->addItem(dev->description(), dev->index());
  }

  const bool migrationDone = settings().value(QStringLiteral(SETTINGS_DEVICE_ID_MIGRATION_DONE), false).toBool();
  if (const std::optional<uint> migratedSinkId = migrateLegacySinkSelection(sinkIds, storedSinkSetting, migrationDone);
      migratedSinkId.has_value()) {
    const QVariant migratedValue = static_cast<uint>(migratedSinkId.value());
    if (settings().value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE) != migratedValue) {
      settings().setValue(QStringLiteral(SETTINGS_DEVICE), migratedValue);
    }
    settings().setValue(QStringLiteral(SETTINGS_DEVICE_ID_MIGRATION_DONE), true);
  }
  else if (!migrationDone && !sinkIds.isEmpty()) {
    settings().setValue(QStringLiteral(SETTINGS_DEVICE_ID_MIGRATION_DONE), true);
  }

  const QVariant resolvedSinkSetting = settings().value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE);
  const uint resolvedSinkId = chooseSinkId(sinkIds, resolvedSinkSetting);

  int comboIndex = 0;
  for (int i = 0; i < ui->devAddedCombo->count(); ++i) {
    if (ui->devAddedCombo->itemData(i).toUInt() == resolvedSinkId) {
      comboIndex = i;
      break;
    }
  }

  ui->devAddedCombo->setCurrentIndex(comboIndex);
  ui->devAddedCombo->blockSignals(oldBlock);

  rebuildPolicyTree(sinks);
}

void OneG4VolumeConfiguration::sinkSelectionChanged(int index) {
  if (mLockSettingChanges) {
    return;
  }

  if (index < 0) {
    settings().setValue(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE);
    return;
  }

  const QVariant data = ui->devAddedCombo->itemData(index);
  settings().setValue(QStringLiteral(SETTINGS_DEVICE), data.isValid() ? data : SETTINGS_DEFAULT_DEVICE);
}

void OneG4VolumeConfiguration::muteOnMiddleClickChanged(bool state) {
  if (!mLockSettingChanges) {
    settings().setValue(QStringLiteral(SETTINGS_MUTE_ON_MIDDLECLICK), state);
  }
}

void OneG4VolumeConfiguration::stepSpinBoxChanged(int step) {
  if (!mLockSettingChanges) {
    settings().setValue(QStringLiteral(SETTINGS_STEP), step);
  }
}

void OneG4VolumeConfiguration::ignoreMaxVolumeCheckBoxChanged(bool state) {
  if (!mLockSettingChanges) {
    settings().setValue(QStringLiteral(SETTINGS_IGNORE_MAX_VOLUME), state);
  }
}

void OneG4VolumeConfiguration::alwaysShowNotificationsCheckBoxChanged(bool state) {
  if (!mLockSettingChanges) {
    settings().setValue(QStringLiteral(SETTINGS_ALWAYS_SHOW_NOTIFICATIONS), state);
  }
}

void OneG4VolumeConfiguration::audioBackendChanged(int index) {
  if (mLockSettingChanges) {
    return;
  }

  if (index < 0) {
    settings().setValue(QStringLiteral(SETTINGS_AUDIO_ENGINE), QStringLiteral(SETTINGS_DEFAULT_AUDIO_ENGINE));
    return;
  }

  const QString backend = ui->audioBackendCombo->itemText(index);
  settings().setValue(QStringLiteral(SETTINGS_AUDIO_ENGINE), backend);
}

void OneG4VolumeConfiguration::loadSettings() {
  mLockSettingChanges = true;

  // Populate audio backend combo
  ui->audioBackendCombo->clear();
  ui->audioBackendCombo->addItem(QLatin1String("PipeWire"));
#ifdef ONEG4_VOLUME_DEV_TEST_BACKENDS
  ui->audioBackendCombo->addItem(QLatin1String("TestBackend"));
#endif

  const QString currentBackend =
      settings().value(QStringLiteral(SETTINGS_AUDIO_ENGINE), QStringLiteral(SETTINGS_DEFAULT_AUDIO_ENGINE)).toString();
  int backendIndex = ui->audioBackendCombo->findText(currentBackend);
  if (backendIndex < 0 && ui->audioBackendCombo->count() > 0) {
    backendIndex = 0;
  }
  if (backendIndex >= 0) {
    ui->audioBackendCombo->setCurrentIndex(backendIndex);
  }

  ui->ignoreMaxVolumeCheckBox->setEnabled(true);

  setComboboxIndexByData(ui->devAddedCombo, settings().value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE),
                         1);

  ui->muteOnMiddleClickCheckBox->setChecked(
      settings().value(QStringLiteral(SETTINGS_MUTE_ON_MIDDLECLICK), SETTINGS_DEFAULT_MUTE_ON_MIDDLECLICK).toBool());

  ui->stepSpinBox->setValue(settings().value(QStringLiteral(SETTINGS_STEP), SETTINGS_DEFAULT_STEP).toInt());

  ui->ignoreMaxVolumeCheckBox->setChecked(
      settings().value(QStringLiteral(SETTINGS_IGNORE_MAX_VOLUME), SETTINGS_DEFAULT_IGNORE_MAX_VOLUME).toBool());

  ui->alwaysShowNotificationsCheckBox->setChecked(
      settings()
          .value(QStringLiteral(SETTINGS_ALWAYS_SHOW_NOTIFICATIONS), SETTINGS_DEFAULT_ALWAYS_SHOW_NOTIFICATIONS)
          .toBool());

  mLockSettingChanges = false;
}

QString OneG4VolumeConfiguration::deriveCardName(const QString& deviceName) {
  if (deviceName.isEmpty())
    return {};

  const QString prefix = deviceName.startsWith(QStringLiteral("alsa_output."))    ? QStringLiteral("alsa_output.")
                         : deviceName.startsWith(QStringLiteral("alsa_input."))   ? QStringLiteral("alsa_input.")
                         : deviceName.startsWith(QStringLiteral("bluez_output.")) ? QStringLiteral("bluez_output.")
                         : deviceName.startsWith(QStringLiteral("bluez_input."))  ? QStringLiteral("bluez_input.")
                                                                                  : QString();

  if (prefix.isEmpty())
    return {};

  QString base = deviceName.mid(prefix.size());
  const int lastDot = base.lastIndexOf(QLatin1Char('.'));
  if (lastDot > 0)
    base = base.left(lastDot);

  const QString cardPrefix =
      prefix.startsWith(QStringLiteral("bluez_")) ? QStringLiteral("bluez_card.") : QStringLiteral("alsa_card.");
  return cardPrefix + base;
}

void OneG4VolumeConfiguration::rebuildPolicyTree(const QList<AudioDevice*>& sinks) {
  if (!m_policy)
    return;

  mUpdatingPolicyTree = true;
  ui->policyTree->clear();

  for (const AudioDevice* dev : std::as_const(sinks)) {
    if (!dev)
      continue;
    const QString nodeName = dev->name();
    const QString cardName = deriveCardName(nodeName);

    auto* item = new QTreeWidgetItem(ui->policyTree);
    item->setText(0, dev->description());
    item->setText(1, nodeName);
    item->setData(0, Qt::UserRole, nodeName);
    item->setData(1, Qt::UserRole, cardName);

    Qt::ItemFlags flags = item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
    item->setFlags(flags);

    const bool deviceDisabled = !cardName.isEmpty() && m_policy->deviceDisabled(cardName);
    const bool nodeDisabled = m_policy->nodeDisabled(nodeName);
    item->setCheckState(2, deviceDisabled ? Qt::Checked : Qt::Unchecked);
    item->setCheckState(3, nodeDisabled ? Qt::Checked : Qt::Unchecked);
    item->setText(2, QString());
    item->setText(3, QString());
  }

  ui->policyTree->resizeColumnToContents(0);
  ui->policyTree->resizeColumnToContents(1);

  mUpdatingPolicyTree = false;
  setPolicyDirty(mPolicyDirty);  // refresh label state
}

void OneG4VolumeConfiguration::policyItemChanged(QTreeWidgetItem* item, int column) {
  if (mUpdatingPolicyTree || !item || !m_policy)
    return;

  const QString nodeName = item->data(0, Qt::UserRole).toString();
  const QString cardName = item->data(1, Qt::UserRole).toString();
  const bool checked = item->checkState(column) == Qt::Checked;

  switch (column) {
    case 2:
      if (!cardName.isEmpty()) {
        m_policy->setDeviceDisabled(cardName, checked);
        setPolicyDirty(true);
      }
      break;
    case 3:
      if (!nodeName.isEmpty()) {
        m_policy->setNodeDisabled(nodeName, checked);
        setPolicyDirty(true);
      }
      break;
    default:
      break;
  }
}

void OneG4VolumeConfiguration::applyPolicy() {
  if (!m_policy)
    return;

  if (!m_policy->write()) {
    QMessageBox::warning(this, tr("WirePlumber policy"), tr("Failed to write WirePlumber policy file."));
    return;
  }

  const QMessageBox::StandardButton restart = QMessageBox::question(
      this, tr("WirePlumber policy"), tr("Policy saved. Restart WirePlumber now to apply changes?"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

  if (restart == QMessageBox::Yes) {
    QString error;
    if (!m_policy->restart(&error)) {
      QMessageBox::warning(this, tr("WirePlumber policy"),
                           error.isEmpty() ? tr("Failed to restart WirePlumber.") : error);
      return;
    }
  }

  setPolicyDirty(false);
}

void OneG4VolumeConfiguration::setPolicyDirty(bool dirty) {
  mPolicyDirty = dirty;
  if (ui->applyPolicyButton)
    ui->applyPolicyButton->setEnabled(dirty);
  if (ui->policyStatusLabel)
    ui->policyStatusLabel->setText(dirty ? tr("Pending restart.") : tr("Saved."));
}
