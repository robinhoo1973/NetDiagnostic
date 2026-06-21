// =============================================================================
// DiagnosticResult.h — Immutable result of a single diagnostic test
// =============================================================================
#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include "DiagId.h"
#include "ResultProperty.h"

struct DiagnosticResult {
    DiagId      id;
    QString     displayName;
    DiagGroup   group;
    TestStatus  status;
    QString     summary;
    QString     details;
    qint64      durationMs = 0;
    QDateTime   timestamp;
    QVector<ResultProperty> properties;
    QString     rawOutput;
    QString     errorOutput;

    // ── Convenience ──────────────────────────────────────────────────────────
    bool isPass()    const { return status == TestStatus::Pass; }
    bool isFail()    const { return status == TestStatus::Fail; }
    bool isWarning() const { return status == TestStatus::Warning; }
    bool isSkipped() const { return status == TestStatus::Skipped; }
    bool isError()   const { return status == TestStatus::Error; }
    bool isInfo()    const { return status == TestStatus::Info; }
    bool isDone()    const { return status != TestStatus::Skipped; }
    QString statusIcon() const { return testStatusIcon(status); }

    // ── Factory helpers ──────────────────────────────────────────────────────
    static DiagnosticResult skipped(DiagId id, const QString& reason);
    static DiagnosticResult error(DiagId id, const QString& msg);
    static DiagnosticResult timeout(DiagId id, DiagGroup group, qint64 durationMs);
};
