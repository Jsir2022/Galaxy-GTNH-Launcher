#pragma once
#include <QString>

QString loadRefreshToken(const QString &clientId);
void saveRefreshToken(const QString &clientId, const QString &token);
