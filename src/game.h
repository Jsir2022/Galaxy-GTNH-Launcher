#pragma once
#include "auth.h"
#include <QString>
#include <functional>

using Progress = std::function<void(QString)>;

void prepareGame(const QString &root, const Progress &status);
void startGame(const QString &root, const Account &account, const Progress &status);
