// =============================================================================
// NetworkProbe.cpp — QTcpSocket/QSslSocket wrappers
// =============================================================================
#include "engine/runner/NetworkProbe.h"
#include <QTcpSocket>
#include <QSslSocket>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QMutex>
#include <QtConcurrent/QtConcurrent>

TcpConnectResult NetworkProbe::tcpConnect(const QString& host, int port, int timeoutMs) {
    TcpConnectResult result;
    QTcpSocket socket;
    QElapsedTimer timer;
    timer.start();
    socket.connectToHost(host, port);
    if (socket.waitForConnected(timeoutMs)) {
        result.connected = true;
        result.latencyMs = timer.elapsed();
    } else {
        result.error = socket.errorString();
    }
    socket.disconnectFromHost();
    return result;
}

QVector<PortScanEntry> NetworkProbe::portScan(const QString& host,
                                               const QVector<int>& commonPorts,
                                               int fromPort, int toPort,
                                               int timeoutMs, int /*maxConcurrent*/) {
    QVector<int> ports;
    if (fromPort > 0 && toPort >= fromPort) {
        fromPort = qBound(1, fromPort, 65535);
        toPort = qBound(fromPort, toPort, 65535);
        for (int p = fromPort; p <= toPort; ++p) ports.append(p);
    } else {
        ports = commonPorts;
    }

    // Synchronous scan — caller is already on a worker thread (DiagnosticEngine::runTest via QtConcurrent)
    QVector<PortScanEntry> results;
    for (int port : ports) {
        TcpConnectResult r = tcpConnect(host, port, timeoutMs);
        PortScanEntry e; e.port = port; e.open = r.connected; e.error = r.error;
        results.append(e);
    }
    std::sort(results.begin(), results.end(),
              [](const PortScanEntry& a, const PortScanEntry& b) { return a.port < b.port; });
    return results;
}

SslCertInfo NetworkProbe::sslCertInfo(const QString& host, int port, int timeoutMs) {
    SslCertInfo info;
    QSslSocket socket;
    socket.connectToHostEncrypted(host, port);
    if (!socket.waitForEncrypted(timeoutMs)) return info;
    const auto certs = socket.peerCertificateChain();
    if (certs.isEmpty()) { socket.disconnectFromHost(); return info; }
    const auto& cert = certs.first();
    info.subject = cert.subjectInfo(QSslCertificate::CommonName).join(", ");
    info.issuer = cert.issuerInfo(QSslCertificate::CommonName).join(", ");
    info.validFrom = cert.effectiveDate();
    info.validTo = cert.expiryDate();
    info.daysLeft = QDateTime::currentDateTime().daysTo(info.validTo);
    info.thumbprint = QString::fromUtf8(cert.digest(QCryptographicHash::Sha256).toHex());
    info.subjectAltNames = cert.subjectAlternativeNames().values();
    info.valid = true;
    socket.disconnectFromHost();
    return info;
}

HttpTimingResult NetworkProbe::httpTiming(const QUrl& url, int timeoutMs) {
    HttpTimingResult result;
    QElapsedTimer totalTimer; totalTimer.start();
    QElapsedTimer t; t.start();
    QHostInfo hi = QHostInfo::fromName(url.host());
    result.dnsMs = t.elapsed();
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setRawHeader("Accept-Encoding", "gzip, deflate");
    QNetworkReply* reply = mgr.get(req);
    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    if (timer.isActive()) {
        timer.stop();
        result.totalMs = totalTimer.elapsed();
        result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.bodyBytes = reply->readAll().size();
        result.firstByteMs = result.totalMs - result.dnsMs;
    } else {
        result.error = "Request timeout";
        reply->abort();
    }
    // reply is child of stack mgr — cleaned up at scope exit, no deleteLater needed
    return result;
}

QVector<int> NetworkProbe::commonDiagnosticPorts() {
    return {21,22,23,25,53,80,110,135,139,143,443,445,993,995,
            1433,1521,1723,3306,3389,5432,5900,6379,8080,8443,27017};
}
