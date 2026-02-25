#include "sinkselection.h"

#include <QtTest/QtTest>

#include <optional>

class SinkSelectionTest : public QObject {
  Q_OBJECT

 private slots:
  void migratesLegacyPositionWhenValueDoesNotMatchAnySinkId();
  void doesNotMigrateWhenValueAlreadyMatchesSinkId();
  void doesNotMigrateWhenMigrationAlreadyDone();
  void doesNotMigrateDefaultSentinelValue();
  void choosesStoredSinkIdWhenPresent();
  void fallsBackToFirstSinkWhenStoredValueIsMissing();
  void choosesObservedDefaultSinkWhenStoredValueDoesNotMatchAnySinkId();
  void keepsStoredSinkWhenStoredValueMatchesSinkIdEvenIfObservedDefaultDiffers();
  void ignoresObservedDefaultWhenNotInCurrentSinkList();
};

void SinkSelectionTest::migratesLegacyPositionWhenValueDoesNotMatchAnySinkId() {
  const QList<uint> sinkIds{42U, 77U, 90U};
  const std::optional<uint> migrated = migrateLegacySinkSelection(sinkIds, QVariant(1), false);

  QVERIFY(migrated.has_value());
  QCOMPARE(migrated.value(), 77U);
}

void SinkSelectionTest::doesNotMigrateWhenValueAlreadyMatchesSinkId() {
  const QList<uint> sinkIds{42U, 77U, 90U};
  const std::optional<uint> migrated = migrateLegacySinkSelection(sinkIds, QVariant(77), false);

  QVERIFY(!migrated.has_value());
}

void SinkSelectionTest::doesNotMigrateWhenMigrationAlreadyDone() {
  const QList<uint> sinkIds{42U, 77U, 90U};
  const std::optional<uint> migrated = migrateLegacySinkSelection(sinkIds, QVariant(1), true);

  QVERIFY(!migrated.has_value());
}

void SinkSelectionTest::doesNotMigrateDefaultSentinelValue() {
  const QList<uint> sinkIds{42U, 77U, 90U};
  const std::optional<uint> migrated = migrateLegacySinkSelection(sinkIds, QVariant(0), false);

  QVERIFY(!migrated.has_value());
}

void SinkSelectionTest::choosesStoredSinkIdWhenPresent() {
  const QList<uint> sinkIds{42U, 77U, 90U};

  QCOMPARE(chooseSinkId(sinkIds, QVariant(90)), 90U);
}

void SinkSelectionTest::fallsBackToFirstSinkWhenStoredValueIsMissing() {
  const QList<uint> sinkIds{42U, 77U, 90U};

  QCOMPARE(chooseSinkId(sinkIds, QVariant(999)), 42U);
  QCOMPARE(chooseSinkId(sinkIds, QVariant(-1)), 42U);
}

void SinkSelectionTest::choosesObservedDefaultSinkWhenStoredValueDoesNotMatchAnySinkId() {
  const QList<uint> sinkIds{42U, 77U, 90U};
  const std::optional<uint> observedDefaultSinkId = 90U;

  QCOMPARE(chooseSinkId(sinkIds, QVariant(999), observedDefaultSinkId), 90U);
}

void SinkSelectionTest::keepsStoredSinkWhenStoredValueMatchesSinkIdEvenIfObservedDefaultDiffers() {
  const QList<uint> sinkIds{42U, 77U, 90U};
  const std::optional<uint> observedDefaultSinkId = 90U;

  QCOMPARE(chooseSinkId(sinkIds, QVariant(42), observedDefaultSinkId), 42U);
}

void SinkSelectionTest::ignoresObservedDefaultWhenNotInCurrentSinkList() {
  const QList<uint> sinkIds{42U, 77U, 90U};
  const std::optional<uint> observedDefaultSinkId = 123U;

  QCOMPARE(chooseSinkId(sinkIds, QVariant(999), observedDefaultSinkId), 42U);
}

QTEST_MAIN(SinkSelectionTest)

#include "sinkselection_test.moc"
