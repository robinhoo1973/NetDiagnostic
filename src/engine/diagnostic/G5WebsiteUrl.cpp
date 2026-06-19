// =============================================================================
// G5WebsiteUrl.cpp — 13 website/protocol diagnostic tests
// =============================================================================
#include "engine/diagnostic/G5WebsiteUrl.h"
#include "engine/runner/NetworkProbe.h"
#include "util/Logger.h"
#include <QUrl>
#include <QHostInfo>
#include <QSslSocket>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>

namespace G5WebsiteUrl {

static const QMap<QString, int> s_defaultPorts = {
    {"http",80},{"https",443},{"ftp",21},{"ftps",990},{"sftp",22},{"ssh",22},
    {"telnet",23},{"rdp",3389},{"smtp",25},{"smtps",465},{"imap",143},{"imaps",993},
    {"pop3",110},{"pop3s",995},{"mysql",3306},{"postgresql",5432},{"redis",6379},
    {"mongodb",27017},{"mssql",1433},{"ldap",389},{"ldaps",636},{"mqtt",1883},{"mqtts",8883}
};

static QUrl validate(const QString& target) {
    QUrl u(target, QUrl::StrictMode);
    if (u.isValid() && !u.scheme().isEmpty())
        return u;
    // Detect bare IPv6 addresses (contain colons but no scheme separator)
    if (target.contains(':') && !target.contains("://")) {
        // Wrap in brackets if needed for IPv6 URL syntax
        QString bracketed = target.startsWith('[') ? target : QStringLiteral("[%1]").arg(target);
        u = QUrl(QStringLiteral("http://") + bracketed, QUrl::StrictMode);
        if (u.isValid()) return u;
    }
    u = QUrl(QStringLiteral("http://") + target);
    return u.isValid() ? u : QUrl();
}

static int portForUrl(const QUrl& u) {
    return u.port() > 0 ? u.port() : s_defaultPorts.value(u.scheme(), 80);
}

static DiagnosticResult g5Result(TestId id, const QString& summary,
                                  TestStatus status = TestStatus::Pass) {
    DiagnosticResult r;
    r.id = id; r.group = TestGroup::G5; r.status = status;
    r.summary = summary; r.timestamp = QDateTime::currentDateTime();
    return r;
}

// ── G5.1 URL Parsing ─────────────────────────────────────────────────────
DiagnosticResult urlParsing(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5UrlParsing, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid()) return g5Result(TestId::G5UrlParsing, "Invalid URL", TestStatus::Fail);
    auto r = g5Result(TestId::G5UrlParsing, "URL parsed");
    r.properties.append(ResultProperty("Scheme", u.scheme()));
    r.properties.append(ResultProperty("Host", u.host()));
    r.properties.append(ResultProperty("Port", QString::number(portForUrl(u))));
    r.properties.append(ResultProperty("Path", u.path()));
    r.properties.append(ResultProperty("Query", u.query()));
    return r;
}

// ── G5.2 TCP Connect ─────────────────────────────────────────────────────
DiagnosticResult tcpConnect(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5TcpConnect, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty())
        return g5Result(TestId::G5TcpConnect, "Invalid target", TestStatus::Fail);
    int port = portForUrl(u);
    auto cr = NetworkProbe::tcpConnect(u.host(), port, 5000);
    auto r = g5Result(TestId::G5TcpConnect,
        cr.connected ? QStringLiteral("Connected in %1ms").arg(cr.latencyMs)
                     : QStringLiteral("Failed: %1").arg(cr.error),
        cr.connected ? TestStatus::Pass : TestStatus::Fail);
    r.properties.append(ResultProperty("Host", u.host()));
    r.properties.append(ResultProperty("Port", QString::number(port)));
    r.durationMs = cr.latencyMs;
    return r;
}

// ── G5.3 Service Banner ──────────────────────────────────────────────────
DiagnosticResult serviceBanner(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5ServiceBanner, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty())
        return g5Result(TestId::G5ServiceBanner, "Invalid target", TestStatus::Fail);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(TestId::G5ServiceBanner, "Connection failed", TestStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.disconnectFromHost();
    auto r = g5Result(TestId::G5ServiceBanner,
        banner.isEmpty() ? "No banner received" : "Banner received",
        banner.isEmpty() ? TestStatus::Warning : TestStatus::Pass);
    r.rawOutput = QString::fromUtf8(banner).left(500);
    return r;
}

// ── G5.4 HTTP Headers ────────────────────────────────────────────────────
DiagnosticResult httpHeaders(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5HttpHeaders, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(TestId::G5HttpHeaders, "Not HTTP(S)", TestStatus::Skipped);
    auto timing = NetworkProbe::httpTiming(u, 15000);
    if (!timing.error.isEmpty())
        return g5Result(TestId::G5HttpHeaders, QStringLiteral("Request failed: %1").arg(timing.error), TestStatus::Fail);
    auto r = g5Result(TestId::G5HttpHeaders,
        QStringLiteral("HTTP %1").arg(timing.statusCode),
        timing.statusCode >= 200 && timing.statusCode < 300 ? TestStatus::Pass :
        timing.statusCode >= 300 && timing.statusCode < 400 ? TestStatus::Warning :
        TestStatus::Fail);
    r.properties.append(ResultProperty("StatusCode", QString::number(timing.statusCode)));
    r.properties.append(ResultProperty("TotalMs", QString::number(timing.totalMs)));
    r.durationMs = timing.totalMs;
    if (!timing.error.isEmpty()) r.errorOutput = timing.error;
    return r;
}

// ── G5.5 Curl Verbose (HTTP request with timing) ─────────────────────────
DiagnosticResult curlVerbose(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5CurlVerbose, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(TestId::G5CurlVerbose, "Not HTTP(S)", TestStatus::Skipped);
    auto timing = NetworkProbe::httpTiming(u, 60000);
    if (!timing.error.isEmpty())
        return g5Result(TestId::G5CurlVerbose, QStringLiteral("Request failed: %1").arg(timing.error), TestStatus::Fail);
    auto r = g5Result(TestId::G5CurlVerbose,
        QStringLiteral("HTTP %1, %2ms total").arg(timing.statusCode).arg(timing.totalMs),
        timing.statusCode >= 200 && timing.statusCode < 400 ? TestStatus::Pass : TestStatus::Fail);
    r.properties.append(ResultProperty("HttpCode", QString::number(timing.statusCode)));
    r.properties.append(ResultProperty("DnsMs", QString::number(timing.dnsMs)));
    r.properties.append(ResultProperty("FirstByteMs", QString::number(timing.firstByteMs)));
    r.properties.append(ResultProperty("TotalMs", QString::number(timing.totalMs)));
    r.properties.append(ResultProperty("BodyBytes", QString::number(timing.bodyBytes)));
    r.durationMs = timing.totalMs;
    if (!timing.error.isEmpty()) r.errorOutput = timing.error;
    return r;
}

// ── G5.6 Security Headers ────────────────────────────────────────────────
DiagnosticResult securityHeaders(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5SecurityHeaders, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(TestId::G5SecurityHeaders, "Not HTTP(S)", TestStatus::Skipped);
    // Use QNetworkAccessManager to get headers
    QNetworkAccessManager mgr;
    QNetworkRequest req(u);
    QNetworkReply* reply = mgr.head(req);
    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();
    QStringList missing;
    if (timer.isActive()) { timer.stop();
        const auto& headers = reply->rawHeaderPairs();
        QStringList found;
        for (const auto& h : headers) found.append(h.first.toLower());
        QStringList required = {"strict-transport-security","content-security-policy",
            "x-frame-options","x-content-type-options","x-xss-protection",
            "referrer-policy","permissions-policy"};
        for (const auto& h : required)
            if (!found.contains(h)) missing.append(h);
    } else { missing = {"timeout"}; reply->abort(); }
    auto r = g5Result(TestId::G5SecurityHeaders,
        missing.isEmpty() ? "All 7 present" : QStringLiteral("%1 missing").arg(missing.size()),
        missing.isEmpty() ? TestStatus::Pass :
        missing.size() <= 4 ? TestStatus::Warning : TestStatus::Fail);
    r.properties.append(ResultProperty("MissingCount", QString::number(missing.size())));
    r.properties.append(ResultProperty("MissingList", missing.join(", ")));
    // reply is child of stack mgr — cleaned up at scope exit, no deleteLater needed
    return r;
}

// ── G5.7 SSL Certificate ─────────────────────────────────────────────────
DiagnosticResult sslCertificate(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5SslCertificate, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "https")
        return g5Result(TestId::G5SslCertificate, "Not HTTPS", TestStatus::Skipped);
    int port = u.port() > 0 ? u.port() : 443;
    auto cert = NetworkProbe::sslCertInfo(u.host(), port, 10000);
    if (!cert.valid)
        return g5Result(TestId::G5SslCertificate, "Failed to get certificate", TestStatus::Fail);
    TestStatus st = TestStatus::Pass;
    QString summary = QStringLiteral("%1 days left").arg(cert.daysLeft);
    if (cert.daysLeft < 0) { st = TestStatus::Fail; summary = "EXPIRED"; }
    else if (cert.daysLeft < 30) { st = TestStatus::Warning; }
    auto r = g5Result(TestId::G5SslCertificate, summary, st);
    r.properties.append(ResultProperty("Subject", cert.subject));
    r.properties.append(ResultProperty("Issuer", cert.issuer));
    r.properties.append(ResultProperty("ValidFrom", cert.validFrom.toString("yyyy-MM-dd")));
    r.properties.append(ResultProperty("ValidTo", cert.validTo.toString("yyyy-MM-dd")));
    r.properties.append(ResultProperty("DaysLeft", QString::number(cert.daysLeft)));
    r.properties.append(ResultProperty("SanCount", QString::number(cert.subjectAltNames.size())));
    r.properties.append(ResultProperty("Thumbprint", cert.thumbprint.left(32) + "..."));
    return r;
}

// ── G5.8 HTTP Redirect ───────────────────────────────────────────────────
DiagnosticResult httpRedirect(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5HttpRedirect, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(TestId::G5HttpRedirect, "Not HTTP(S)", TestStatus::Skipped);
    // Simple redirect check — single HEAD, check for 3xx
    QNetworkAccessManager mgr;
    mgr.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    QNetworkRequest req(u);
    QNetworkReply* reply = mgr.head(req);
    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();
    auto r = g5Result(TestId::G5HttpRedirect, "No redirect");
    if (timer.isActive()) { timer.stop();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString location = reply->rawHeader("Location");
        if (code >= 300 && code < 400 && !location.isEmpty()) {
            r.summary = QStringLiteral("Redirect %1 → %2").arg(code).arg(location);
            r.status = TestStatus::Warning;
            r.properties.append(ResultProperty("RedirectCount", "1"));
            r.properties.append(ResultProperty("FinalUrl", location));
        } else {
            r.properties.append(ResultProperty("RedirectCount", "0"));
        }
    } else { r.summary = "Timeout"; r.status = TestStatus::Error; reply->abort(); }
    // reply is child of stack mgr — cleaned up at scope exit, no deleteLater needed
    return r;
}

// ── Remaining tests (stubs for Phase 4) ──────────────────────────────────

DiagnosticResult httpCompression(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5HttpCompression, "No target", TestStatus::Skipped);
    return g5Result(TestId::G5HttpCompression, "Phase 4 — basic check", TestStatus::Info);
}

DiagnosticResult httpTiming(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5HttpTiming, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(TestId::G5HttpTiming, "Not HTTP(S)", TestStatus::Skipped);
    auto timing = NetworkProbe::httpTiming(u, 30000);
    auto r = g5Result(TestId::G5HttpTiming,
        QStringLiteral("%1ms total").arg(timing.totalMs),
        timing.totalMs < 1000 ? TestStatus::Pass :
        timing.totalMs < 3000 ? TestStatus::Warning : TestStatus::Fail);
    r.properties.append(ResultProperty("DnsMs", QString::number(timing.dnsMs)));
    r.properties.append(ResultProperty("FirstByteMs", QString::number(timing.firstByteMs)));
    r.properties.append(ResultProperty("TotalMs", QString::number(timing.totalMs)));
    r.durationMs = timing.totalMs;
    return r;
}

DiagnosticResult ftpDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5FtpDiagnostics, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "ftp" && u.scheme() != "ftps")
        return g5Result(TestId::G5FtpDiagnostics, "Not FTP", TestStatus::Skipped);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(TestId::G5FtpDiagnostics, "Connection failed", TestStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    return g5Result(TestId::G5FtpDiagnostics,
        banner.isEmpty() ? "No banner" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? TestStatus::Warning : TestStatus::Pass);
}

DiagnosticResult sshDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5SshDiagnostics, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "ssh" && u.scheme() != "sftp")
        return g5Result(TestId::G5SshDiagnostics, "Not SSH", TestStatus::Skipped);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(TestId::G5SshDiagnostics, "Connection failed", TestStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.disconnectFromHost();
    QString bstr = QString::fromUtf8(banner).trimmed().left(200);
    QString version;
    if (bstr.startsWith("SSH-")) version = bstr.section(' ', 0, 0);
    return g5Result(TestId::G5SshDiagnostics,
        version.isEmpty() ? "No SSH banner" : version,
        version.isEmpty() ? TestStatus::Warning : TestStatus::Pass);
}

DiagnosticResult emailDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(TestId::G5EmailDiagnostics, "No target", TestStatus::Skipped);
    QUrl u = validate(target);
    QString scheme = u.scheme();
    if (scheme != "smtp" && scheme != "imap" && scheme != "pop3"
        && scheme != "smtps" && scheme != "imaps" && scheme != "pop3s")
        return g5Result(TestId::G5EmailDiagnostics,
                         "Not email protocol (smtp/smtps/imap/imaps/pop3/pop3s)", TestStatus::Skipped);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(TestId::G5EmailDiagnostics, "Connection failed", TestStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.disconnectFromHost();
    return g5Result(TestId::G5EmailDiagnostics,
        banner.isEmpty() ? "No banner" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? TestStatus::Warning : TestStatus::Pass);
}

} // namespace G5WebsiteUrl
