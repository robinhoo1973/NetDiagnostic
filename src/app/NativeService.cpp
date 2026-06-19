// =============================================================================
// NativeService.cpp
// =============================================================================
#include "app/NativeService.h"
#include "nd_plugin.h"
#include "nd_types.h"
#include "util/Logger.h"
#include <cstring>

NativeService& NativeService::instance() {
    static NativeService inst;
    return inst;
}

NativeService::~NativeService() {
    shutdown();
}

bool NativeService::initialize() {
    if (m_available) return true;

    int rc = nd_init();
    if (rc != ND_OK) {
        Logger::instance().error(QStringLiteral("nd_init() failed: %1").arg(rc));
        return false;
    }

    const char* ver = nd_version();
    if (ver) {
        m_version = QString::fromUtf8(ver);
        // Extract and validate major version
        auto parts = m_version.split('.');
        if (!parts.isEmpty()) {
            bool ok = false;
            m_versionMajor = parts[0].toInt(&ok);
            if (!ok) {
                Logger::instance().warn(QStringLiteral("Native plugin version unparseable: '%1'").arg(m_version));
            } else if (m_versionMajor != ND_VERSION_MAJOR) {
                Logger::instance().warn(
                    QStringLiteral("Native plugin version mismatch: got v%1, expected v%2.x")
                        .arg(m_version).arg(ND_VERSION_MAJOR));
            }
        }
        Logger::instance().event(QStringLiteral("Native plugin v%1 loaded").arg(m_version));
    }

    m_available = true;
    return true;
}

void NativeService::shutdown() {
    if (m_available) {
        nd_shutdown();
        m_available = false;
    }
}

bool NativeService::isNativeCapable(TestId id) const {
    return testIdToNativeId(id) >= 0;
}

std::optional<DiagnosticResult> NativeService::runDiagnostic(TestId id, const QString& target,
                                                           int fromPort, int toPort) {
    if (!m_available) return std::nullopt;

    int nativeId = testIdToNativeId(id);
    if (nativeId < 0) return std::nullopt;

    NdDiagnosticInput input{};
    input.diag_id = static_cast<NdDiagnosticId>(nativeId);
    QByteArray targetUtf8 = target.toUtf8();
    input.target = targetUtf8.isEmpty() ? nullptr : targetUtf8.constData();
    input.timeout_ms = testGroupTimeoutSec(testGroup(id)) * 1000;
    input.from_port = fromPort;
    input.to_port = toPort;

    fprintf(stderr, "[TRACE] NativeService calling nd_run_diagnostic nativeId=%d\n", nativeId);
    NdDiagnosticResult* nr = nd_run_diagnostic(&input);
    if (!nr) {
        Logger::instance().error(QStringLiteral("nd_run_diagnostic(%1) returned null").arg(nativeId));
        return std::nullopt;
    }

    // Fall back to C++/Qt implementation for not-supported and internal errors
    if (nr->error_code == ND_ERR_NOT_SUPPORTED || nr->error_code == ND_ERR_INTERNAL) {
        nd_free_result(nr);
        return std::nullopt;
    }

    DiagnosticResult result = mapResult(nr);
    nd_free_result(nr);
    return result;
}

DiagnosticResult NativeService::mapResult(const NdDiagnosticResult* nr) const {
    DiagnosticResult r;
    r.id = TestId::G1NetworkAdapters; // placeholder, caller overrides

    // Map status
    switch (nr->status) {
        case ND_STATUS_PASS:    r.status = TestStatus::Pass;    break;
        case ND_STATUS_WARNING: r.status = TestStatus::Warning; break;
        case ND_STATUS_FAIL:    r.status = TestStatus::Fail;    break;
        case ND_STATUS_SKIPPED: r.status = TestStatus::Skipped; break;
        case ND_STATUS_ERROR:   r.status = TestStatus::Error;   break;
        case ND_STATUS_INFO:    r.status = TestStatus::Info;    break;
        default:                r.status = TestStatus::Error;   break;
    }

    if (nr->summary)   r.summary    = QString::fromUtf8(nr->summary);
    if (nr->details)   r.details    = QString::fromUtf8(nr->details);
    if (nr->raw_output) r.rawOutput = QString::fromUtf8(nr->raw_output);
    if (nr->error_msg)  r.errorOutput = QString::fromUtf8(nr->error_msg);

    r.durationMs = nr->duration_us / 1000;
    r.timestamp = QDateTime::currentDateTime();

    // Map properties
    for (int i = 0; i < nr->property_count; ++i) {
        ResultProperty prop;
        if (nr->properties[i].label) prop.label = QString::fromUtf8(nr->properties[i].label);
        if (nr->properties[i].value) prop.value = QString::fromUtf8(nr->properties[i].value);
        r.properties.append(prop);
    }

    return r;
}
