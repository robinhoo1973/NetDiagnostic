// =============================================================================
// PlatformCommand.h — Abstract OS command runner (strategy pattern)
//
// Implementations: PlatformCommandUnix (bash), PlatformCommandWin (PowerShell),
//                  PlatformCommandMac (zsh).
// =============================================================================
#pragma once

#include <QString>
#include <QStringList>
#include <memory>

class QProcess;

struct ProcessResult {
    QString stdoutStr;
    QString stderrStr;
    int exitCode = -1;
    bool timedOut = false;

    bool isSuccess() const { return exitCode == 0 && !timedOut; }
};

class PlatformCommand {
public:
    virtual ~PlatformCommand() = default;

    /// Shell binary path (e.g. /bin/bash, powershell.exe, /bin/zsh)
    virtual QString shellExecutable() const = 0;

    /// Arguments to pass a command string to the shell
    virtual QStringList shellArguments(const QString& command) const = 0;

    /// Locate an executable on PATH. Returns empty string if not found.
    virtual QString findExecutable(const QString& name) const = 0;

    /// One-shot: run a command and wait for completion with timeout (ms).
    /// Never throws — errors are captured in ProcessResult.
    ProcessResult runAndWait(const QString& executable,
                             const QStringList& arguments,
                             int timeoutMs = 30000) const;

    /// Create a QProcess for streaming output (caller owns the process).
    QProcess* runStream(const QString& executable,
                        const QStringList& arguments) const;
};

// ── Factory ─────────────────────────────────────────────────────────────────
std::unique_ptr<PlatformCommand> createPlatformCommand();
