// =============================================================================
// DiagnosticResult.cpp — Factory helper implementations
// =============================================================================
#include "models/DiagnosticResult.h"

DiagnosticResult DiagnosticResult::skipped(TestId id, const QString& reason) {
    DiagnosticResult r;
    r.id = id;
    r.group = testGroup(id);
    r.status = TestStatus::Skipped;
    r.summary = reason;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

DiagnosticResult DiagnosticResult::error(TestId id, const QString& msg) {
    DiagnosticResult r;
    r.id = id;
    r.group = testGroup(id);
    r.status = TestStatus::Error;
    r.summary = msg;
    r.errorOutput = msg;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

DiagnosticResult DiagnosticResult::timeout(TestId id, TestGroup group, qint64 durationMs) {
    DiagnosticResult r;
    r.id = id;
    r.group = group;
    r.status = TestStatus::Error;
    r.summary = QStringLiteral("Timeout (%1s)").arg(durationMs / 1000);
    r.durationMs = durationMs;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}
