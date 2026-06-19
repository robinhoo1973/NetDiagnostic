// =============================================================================
// DiagnosticEngine.h — Orchestrator: dispatches tests, native→Qt fallback
// =============================================================================
#pragma once

#include <QObject>
#include <QFuture>
#include <memory>
#include <atomic>
#include "models/TestId.h"
#include "models/DiagnosticResult.h"
#include "engine/PlatformCommand.h"

class DiagnosticEngine : public QObject {
    Q_OBJECT
public:
    explicit DiagnosticEngine(std::unique_ptr<PlatformCommand> cmd, QObject* parent = nullptr);
    ~DiagnosticEngine() override;

    /// Run a single diagnostic test. Port params only used for G4PortScan.
    QFuture<DiagnosticResult> runTest(TestId id, const QString& target = {},
                                       int fromPort = 0, int toPort = 0,
                                       bool useCommonPorts = true);

    /// The platform command runner (for G4/G5 direct use).
    PlatformCommand* platformCommand() const { return m_cmd.get(); }

private:
    DiagnosticResult runG1(TestId id, const QString& target);
    DiagnosticResult runG2(TestId id, const QString& target);
    DiagnosticResult runG3(TestId id, const QString& target);
    DiagnosticResult runG4(TestId id, const QString& target, int fromPort, int toPort, bool useCommonPorts);
    DiagnosticResult runG5(TestId id, const QString& target);

    /// Try native plugin first; if unavailable/unsupported, returns nullopt.
    std::optional<DiagnosticResult> tryNative(TestId id, const QString& target,
                                               int fromPort = 0, int toPort = 0);

    std::unique_ptr<PlatformCommand> m_cmd;
    std::atomic<bool> m_destroying{false};
};
