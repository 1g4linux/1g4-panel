/* plugin-volume/sinkselection.cpp
 * Sink selection and migration helpers
 */

#include "sinkselection.h"

#include <QtGlobal>

namespace {

bool toNonNegativeInt(const QVariant& value, int* out) {
  Q_ASSERT(out != nullptr);

  bool ok = false;
  const int converted = value.toInt(&ok);
  if (!ok || converted < 0) {
    return false;
  }

  *out = converted;
  return true;
}

}  // namespace

std::optional<uint> migrateLegacySinkSelection(const QList<uint>& sinkIds,
                                               const QVariant& storedValue,
                                               bool migrationAlreadyDone) {
  if (migrationAlreadyDone || sinkIds.isEmpty()) {
    return std::nullopt;
  }

  int legacyIndex = 0;
  if (!toNonNegativeInt(storedValue, &legacyIndex)) {
    return std::nullopt;
  }

  // Keep the default sentinel value stable so "auto select" mode can be
  // resolved against live backend defaults at runtime.
  if (legacyIndex == 0) {
    return std::nullopt;
  }

  if (legacyIndex >= sinkIds.size()) {
    return std::nullopt;
  }

  const uint legacyAsId = static_cast<uint>(legacyIndex);
  if (sinkIds.contains(legacyAsId)) {
    return std::nullopt;
  }

  return sinkIds.at(legacyIndex);
}

uint chooseSinkId(const QList<uint>& sinkIds, const QVariant& storedValue) {
  return chooseSinkId(sinkIds, storedValue, std::nullopt);
}

uint chooseSinkId(const QList<uint>& sinkIds,
                  const QVariant& storedValue,
                  const std::optional<uint>& observedDefaultSinkId) {
  if (sinkIds.isEmpty()) {
    return 0U;
  }

  int storedIndex = 0;
  if (toNonNegativeInt(storedValue, &storedIndex)) {
    const uint sinkId = static_cast<uint>(storedIndex);
    if (sinkIds.contains(sinkId)) {
      return sinkId;
    }
  }

  if (observedDefaultSinkId.has_value() && sinkIds.contains(observedDefaultSinkId.value())) {
    return observedDefaultSinkId.value();
  }

  return sinkIds.first();
}
