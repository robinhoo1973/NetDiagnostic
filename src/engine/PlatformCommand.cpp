// =============================================================================
// PlatformCommand.cpp — Base implementation + platform factory
// =============================================================================
#include "engine/PlatformCommand.h"
#include <QProcess>
#include <QThread>
#include <QElapsedTimer>
#include <QDebug>

#if defined(Q_OS_WIN)
#include <QStandardPaths>
#elif defined(Q_OS_MACOS)
#include <QStandardPaths>
#else
#include <QStandardPaths>
#endif

// ── Base implementation ─────────────────────────────────────────────────────

ProcessResult PlatformCommand::runAndWait(const QString& executable,
                                          const QStringList& arguments,
                                          int timeoutMs) const
{
    ProcessResult result;

    QProcess proc;
    // Ensure QProcess is created in the calling thread (safe for worker threads)
    proc.moveToThread(QThread::currentThread());
    proc.start(executable, arguments);

    if (!proc.waitForStarted(5000)) {
        result.stderrStr = QStringLiteral("Failed to start: %1").arg(proc.errorString());
        result.exitCode = -1;
        return result;
    }

    bool finished = proc.waitForFinished(timeoutMs);
    if (!finished) {
        proc.kill();
        proc.waitForFinished(3000);
        result.timedOut = true;
    }

    result.exitCode = proc.exitCode();
    result.stdoutStr = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    result.stderrStr = QString::fromUtf8(proc.readAllStandardError()).trimmed();

    return result;
}

QProcess* PlatformCommand::runStream(const QString& executable,
                                      const QStringList& arguments) const
{
    auto* proc = new QProcess();
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->start(executable, arguments);
    return proc;
}

// ── Linux: bash ────────────────────────────────────────────────────────────

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

class PlatformCommandUnix final : public PlatformCommand {
public:
    QString shellExecutable() const override { return QStringLiteral("/bin/bash"); }
    QStringList shellArguments(const QString& cmd) const override {
        return { QStringLiteral("-c"), cmd };
    }
    QString findExecutable(const QString& name) const override {
        ProcessResult r = runAndWait(QStringLiteral("which"), {name}, 5000);
        return r.isSuccess() ? r.stdoutStr.trimmed() : QString();
    }
};

#elif defined(Q_OS_ANDROID)

// Android: use /system/bin/sh (bash not guaranteed)
class PlatformCommandUnix final : public PlatformCommand {
public:
    QString shellExecutable() const override { return QStringLiteral("/system/bin/sh"); }
    QStringList shellArguments(const QString& cmd) const override {
        return { QStringLiteral("-c"), cmd };
    }
    QString findExecutable(const QString& name) const override {
        ProcessResult r = runAndWait(QStringLiteral("which"), {name}, 5000);
        return r.isSuccess() ? r.stdoutStr.trimmed() : QString();
    }
};

#elif defined(Q_OS_IOS)

// iOS: no shell access — stub implementation
class PlatformCommandStub final : public PlatformCommand {
public:
    QString shellExecutable() const override { return {}; }
    QStringList shellArguments(const QString&) const override { return {}; }
    QString findExecutable(const QString&) const override { return {}; }
    ProcessResult runAndWait(const QString&, const QStringList&, int) const override {
        ProcessResult r; r.exitCode = -1; r.timedOut = false;
        r.stdoutStr = QString(); r.stderrStr = QStringLiteral("Shell not available on this platform");
        return r;
    }
};

// ── macOS: zsh ─────────────────────────────────────────────────────────────

#elif defined(Q_OS_MACOS)

class PlatformCommandMac final : public PlatformCommand {
public:
    QString shellExecutable() const override { return QStringLiteral("/bin/zsh"); }
    QStringList shellArguments(const QString& cmd) const override {
        return { QStringLiteral("-c"), cmd };
    }
    QString findExecutable(const QString& name) const override {
        ProcessResult r = runAndWait(QStringLiteral("which"), {name}, 5000);
        return r.isSuccess() ? r.stdoutStr.trimmed() : QString();
    }
};

// ── Windows: PowerShell ────────────────────────────────────────────────────

#elif defined(Q_OS_WIN)

class PlatformCommandWin final : public PlatformCommand {
public:
    QString shellExecutable() const override { return QStringLiteral("powershell.exe"); }
    QStringList shellArguments(const QString& cmd) const override {
        return { QStringLiteral("-NoProfile"), QStringLiteral("-Command"), cmd };
    }
    QString findExecutable(const QString& name) const override {
        // Try 'where' first, then 'Get-Command'
        ProcessResult r = runAndWait(QStringLiteral("where"), {name}, 5000);
        if (r.isSuccess()) return r.stdoutStr.trimmed();
        r = runAndWait(QStringLiteral("powershell.exe"),
                       {QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
                        QStringLiteral("(Get-Command %1).Source").arg(name)}, 5000);
        return r.isSuccess() ? r.stdoutStr.trimmed() : QString();
    }
};

#else
// fallback for other Unix (same as Linux)
class PlatformCommandUnix final : public PlatformCommand {
public:
    QString shellExecutable() const override { return QStringLiteral("/bin/sh"); }
    QStringList shellArguments(const QString& cmd) const override {
        return { QStringLiteral("-c"), cmd };
    }
    QString findExecutable(const QString& name) const override {
        ProcessResult r = runAndWait(QStringLiteral("which"), {name}, 5000);
        return r.isSuccess() ? r.stdoutStr.trimmed() : QString();
    }
};

#endif

// ── Factory ─────────────────────────────────────────────────────────────────

std::unique_ptr<PlatformCommand> createPlatformCommand() {
#if defined(Q_OS_WIN)
    return std::make_unique<PlatformCommandWin>();
#elif defined(Q_OS_MACOS)
    return std::make_unique<PlatformCommandMac>();
#elif defined(Q_OS_IOS)
    return std::make_unique<PlatformCommandStub>();
#else
    return std::make_unique<PlatformCommandUnix>();
#endif
}
