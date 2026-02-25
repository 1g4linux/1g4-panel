/* plugin-volume/sinkselection.h
 * Sink selection and migration helpers
 */

#ifndef SINKSELECTION_H
#define SINKSELECTION_H

#include <QList>
#include <QVariant>

#include <optional>

std::optional<uint> migrateLegacySinkSelection(const QList<uint>& sinkIds,
                                               const QVariant& storedValue,
                                               bool migrationAlreadyDone);

uint chooseSinkId(const QList<uint>& sinkIds, const QVariant& storedValue);
uint chooseSinkId(const QList<uint>& sinkIds,
                  const QVariant& storedValue,
                  const std::optional<uint>& observedDefaultSinkId);

#endif  // SINKSELECTION_H
