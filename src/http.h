#pragma once
#include <QCryptographicHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <functional>

[[noreturn]] void fail(const QString &message);
QJsonObject readJson(const QByteArray &bytes);
QByteArray readFile(const QString &path);
void makeDirectory(const QString &path);

struct Download {
    QUrl url;
    QString path;
    QByteArray digest;
    qint64 size;
    QCryptographicHash::Algorithm algorithm = QCryptographicHash::Sha1;
};

class Http {
public:
    Http();
    QJsonObject json(const QUrl &url, const QByteArray &body = {},
                     const QByteArray &contentType = {}, const QByteArray &token = {},
                     bool allowHttpError = false);
    void download(const Download &file, const std::function<void(qint64)> &progress = {});
private:
    QNetworkAccessManager manager;
};
