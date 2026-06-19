// Quick verification that native plugin + engine integrate correctly.
// Build: cd build && cmake .. -DBUILD_TESTS=ON && cmake --build . && ctest -V
#include <QCoreApplication>
#include <QDebug>
#include "app/NativeService.h"
#include "engine/PlatformCommand.h"
#include "engine/diagnostic/DiagnosticEngine.h"
#include "models/DiagnosticResult.h"
#include "util/Logger.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    Logger::instance().info("Test: Starting engine smoke test");

    // 1. Verify native plugin initializes
    auto& ns = NativeService::instance();
    bool ok = ns.initialize();
    qDebug() << "Native plugin initialized:" << ok << "version:" << ns.version();

    // 2. Create PlatformCommand
    auto cmd = createPlatformCommand();
    qDebug() << "PlatformCommand shell:" << cmd->shellExecutable();

    // 3. Create DiagnosticEngine
    DiagnosticEngine engine(std::move(cmd));

    // 4. Run a native test (G1 NetworkAdapters)
    qDebug() << "Running G1 NetworkAdapters...";
    auto future = engine.runTest(TestId::G1NetworkAdapters);
    future.waitForFinished();
    auto result = future.result();
    qDebug() << "  id:" << static_cast<int>(result.id)
             << "status:" << static_cast<int>(result.status)
             << "summary:" << result.summary
             << "durationMs:" << result.durationMs;

    // 5. Run a non-native test (G5 URL parsing — should skip)
    qDebug() << "Running G5 URL Parsing (no native, no Qt impl yet)...";
    auto future2 = engine.runTest(TestId::G5UrlParsing, "https://example.com");
    future2.waitForFinished();
    auto result2 = future2.result();
    qDebug() << "  id:" << static_cast<int>(result2.id)
             << "status:" << static_cast<int>(result2.status)
             << "summary:" << result2.summary;

    ns.shutdown();
    Logger::instance().info("Test: Complete");
    return 0;
}
