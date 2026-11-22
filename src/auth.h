#pragma once
#include <QString>
#include <QUrl>
#include <functional>

struct Account {
    QString name;
    QString uuid;
    QString accessToken;
};

Account authenticate(const QString &clientId, const std::function<void(QUrl, QString)> &authorize);
