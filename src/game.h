#pragma once
#include "auth.h"
#include <QString>
#include <functional>

using Progress = std::function<void(QString)>;

enum class DownloadSource { Mojang, Bmclapi };

void prepareGame(const QString &root, DownloadSource source, const Progress &status);
void startGame(const QString &root, const Account &account, const Progress &status);
