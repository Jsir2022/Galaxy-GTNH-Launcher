#include "http.h"
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkProxyFactory>
#include <QNetworkProxy>
#include <QFileInfo>
#include <QNetworkReply>
#include <QSaveFile>
#include <QThread>
#include <memory>
#include <stdexcept>

void fail(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}

QJsonObject readJson(const QByteArray &bytes)
{
    const auto document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) fail("无效的 JSON 响应。");
    return document.object();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) fail("无法读取 " + path);
    return file.readAll();
}

void makeDirectory(const QString &path)
{
    if (!QDir().mkpath(path)) fail("无法创建目录 " + path);
}

Http::Http()
{
    manager.setTransferTimeout(45000);
    QUrl proxy(qEnvironmentVariable("HTTPS_PROXY", qEnvironmentVariable("https_proxy")));
    if (proxy.isValid() && proxy.scheme() == "http" && !proxy.host().isEmpty()) {
        manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, proxy.host(), proxy.port(80),
                                       proxy.userName(), proxy.password()));
    }
}

static QNetworkRequest requestFor(const QUrl &url, QNetworkRequest::RedirectPolicy policy)
{
    if (url.scheme() != "https" || url.host().isEmpty()) fail("下载或授权地址必须使用 HTTPS。");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, policy);
    request.setRawHeader("User-Agent", "GTNH-Launcher/0.1");
    return request;
}

static void await(QNetworkReply *reply)
{
    if (reply->isFinished()) return;
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
}

QJsonObject Http::json(const QUrl &url, const QByteArray &body,
                       const QByteArray &contentType, const QByteArray &token, bool allowHttpError)
{
    auto request = requestFor(url, QNetworkRequest::ManualRedirectPolicy);
    if (!contentType.isEmpty()) request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (!token.isEmpty()) request.setRawHeader("Authorization", "Bearer " + token);
    std::unique_ptr<QNetworkReply> reply(body.isEmpty() ? manager.get(request) : manager.post(request, body));
    QByteArray data;
    auto receive = [&] {
        data += reply->readAll();
        if (data.size() > 1024 * 1024) reply->abort();
    };
    QObject::connect(reply.get(), &QNetworkReply::readyRead, reply.get(), receive);
    await(reply.get());
    receive();
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (data.size() > 1024 * 1024 || status < 200 || (status >= 300 && status < 400) ||
        (!allowHttpError && status >= 300) || (status < 300 && reply->error() != QNetworkReply::NoError)) {
        fail(QString("授权请求失败（%1，HTTP %2）。").arg(url.host()).arg(status));
    }
    return readJson(data);
}

static bool transientDownloadError(QNetworkReply::NetworkError error, int status)
{
    return status == 408 || status == 429 || (status >= 500 && status < 600) ||
           error == QNetworkReply::RemoteHostClosedError || error == QNetworkReply::TimeoutError ||
           error == QNetworkReply::TemporaryNetworkFailureError || error == QNetworkReply::ProxyTimeoutError ||
           error == QNetworkReply::ProxyConnectionClosedError;
}

void Http::download(const Download &file, const std::function<void(qint64)> &progress)
{
    QFile existing(file.path);
    if (existing.size() == file.size && existing.open(QIODevice::ReadOnly)) {
        QCryptographicHash hash(file.algorithm);
        if (hash.addData(&existing) && hash.result().toHex() == file.digest) return;
    }
    existing.close();
    makeDirectory(QFileInfo(file.path).absolutePath());
    constexpr int maxAttempts = 3;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        QSaveFile output(file.path);
        if (!output.open(QIODevice::WriteOnly)) fail("无法写入 " + file.path);
        QCryptographicHash hash(file.algorithm);
        auto request = requestFor(file.url, QNetworkRequest::NoLessSafeRedirectPolicy);
        std::unique_ptr<QNetworkReply> reply(manager.get(request));
        qint64 size = 0;
        bool writeError = false;
        auto receive = [&] {
            if (reply->bytesAvailable() == 0) return;
            const auto bytes = reply->readAll();
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 400) return;
            size += bytes.size();
            if (size > file.size || output.write(bytes) != bytes.size()) {
                writeError = true;
                reply->abort();
                return;
            }
            hash.addData(bytes);
            if (progress) progress(size);
        };
        QObject::connect(reply.get(), &QNetworkReply::readyRead, reply.get(), receive);
        await(reply.get());
        receive();
        if (writeError) fail("下载超出预期大小或无法写入 " + file.path);
        if (reply->error() != QNetworkReply::NoError) {
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (attempt < maxAttempts && transientDownloadError(reply->error(), status)) {
                output.cancelWriting();
                reply.reset();
                manager.clearConnectionCache();
                QThread::msleep(500 * attempt);
                continue;
            }
            fail("下载失败 " + file.url.toString() + " · " + reply->errorString());
        }
        if (size != file.size || hash.result().toHex() != file.digest)
            fail("文件校验失败 " + file.url.toString());
        if (!output.commit()) fail("无法保存 " + file.path);
        return;
    }
}
