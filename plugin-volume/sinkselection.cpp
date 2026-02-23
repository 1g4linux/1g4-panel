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

  return sinkIds.first();
}
