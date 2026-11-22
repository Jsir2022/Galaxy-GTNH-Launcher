#include "game.h"
#include "config.h"
#include "http.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <array>
#include <future>

#ifdef Q_OS_WIN
static const QString platform = "windows";
static const QString javaName = "java.exe";
static const QChar classpathSeparator = ';';
#else
static const QString platform = "linux";
static const QString javaName = "java";
static const QChar classpathSeparator = ':';
#endif

static QJsonObject metadata()
{
    return readJson(readFile(":/pack.json"));
}

static QString appPath()
{
    return QCoreApplication::applicationDirPath();
}

static QString javaPath()
{
    const auto java = appPath() + "/runtime/bin/" + javaName;
    if (!QFileInfo(java).isExecutable()) fail("发行包缺少内置 Java 25。请完整解压启动器。");
    return java;
}

static void extract(const QString &zip, const QString &directory, const QString &prefix,
                    const QStringList &exclude = {})
{
    makeDirectory(directory);
    QProcess java;
    java.start(javaPath(), QStringList{"-cp", appPath(), "Extract", zip, directory, prefix} + exclude);
    if (!java.waitForStarted() || !java.waitForFinished(180000) ||
        java.exitStatus() != QProcess::NormalExit || java.exitCode() != 0) {
        fail("解压失败：" + QString::fromUtf8(java.readAllStandardError()).left(1600));
    }
}

static Download fileSpec(const QString &path, const QJsonObject &record, bool archive = false)
{
    return {QUrl(record.value("url").toString()), path,
            record.value(archive ? "sha256" : "sha1").toString().toLatin1(),
            record.value("size").toInteger(),
            archive ? QCryptographicHash::Sha256 : QCryptographicHash::Sha1};
}

static QStringList strings(const QJsonValue &value)
{
    QStringList result;
    for (const auto &item : value.toArray()) result.append(item.toString());
    return result;
}

static void installPack(const QString &root, const QJsonObject &meta, const Progress &status)
{
    const auto final = root + "/pack";
    const auto archive = fileSpec(root + "/gtnh-2.9.0.zip", meta.value("archive").toObject(), true);
    if (QFileInfo::exists(final)) {
        if (readFile(final + "/.launcher-pack").trimmed() != archive.digest)
            fail("该目录的整合包与此发行版不一致，请选择一个新目录。");
        return;
    }
    status("正在下载 GTNH 2.9.0 整合包…");
    Http http;
    QElapsedTimer timer;
    timer.start();
    http.download(archive, [&](qint64 received) {
        if (timer.elapsed() < 250) return;
        timer.restart();
        status(QString("正在下载 GTNH 2.9.0 · %1%").arg(received * 100 / archive.size));
    });
    status("正在解压 GTNH 整合包…");
    QTemporaryDir stage(root + "/.pack-XXXXXX");
    if (!stage.isValid()) fail("无法建立整合包安装临时目录。");
    extract(archive.path, stage.path(), meta.value("instance").toString());
    if (!QFileInfo(stage.filePath(".minecraft/mods")).isDir())
        fail("整合包缺少 Minecraft 模组目录。");
    QFile marker(stage.filePath(".launcher-pack"));
    if (!marker.open(QIODevice::WriteOnly) || marker.write(archive.digest) != archive.digest.size() ||
        !marker.flush()) fail("无法写入安装标记。");
    marker.close();
    if (!QDir().rename(stage.path(), final)) fail("无法提交整合包安装。");
    stage.setAutoRemove(false);
    QFile::remove(archive.path);
}

static void downloadBatch(const QList<Download> &files)
{
    if (files.isEmpty()) return;
    // Each worker owns its own Qt network manager and reuses connections across its queue.
    constexpr int parallelism = 8;
    std::array<std::future<void>, parallelism> jobs;
    for (int worker = 0; worker < parallelism && worker < files.size(); ++worker) {
        jobs[worker] = std::async(std::launch::async, [&files, worker] {
            Http http;
            for (qsizetype index = worker; index < files.size(); index += parallelism)
                http.download(files[index]);
        });
    }
    for (auto &job : jobs) {
        if (job.valid()) job.get();
    }
}

static void installAssets(const QString &root, const QJsonObject &index, const Progress &status)
{
    const auto assets = root + "/assets";
    const auto id = index.value("id").toString();
    const auto indexPath = assets + "/indexes/" + id + ".json";
    Http http;
    http.download(fileSpec(indexPath, index));
    const auto metadata = readJson(readFile(indexPath));
    const auto objects = metadata.value("objects").toObject();
    if (objects.isEmpty()) fail("游戏资源索引为空。");
    QSet<QString> queued;
    QList<Download> batch;
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        const auto object = it.value().toObject();
        const auto hash = object.value("hash").toString();
        if (!QRegularExpression("^[a-f0-9]{40}$").match(hash).hasMatch())
            fail("游戏资源索引包含无效摘要。");
        if (queued.contains(hash)) continue;
        queued.insert(hash);
        const auto relative = hash.left(2) + "/" + hash;
        batch.append({QUrl("https://resources.download.minecraft.net/" + relative),
                      assets + "/objects/" + relative, hash.toLatin1(), object.value("size").toInteger()});
    }
    constexpr int batchSize = 128;
    for (qsizetype offset = 0; offset < batch.size(); offset += batchSize) {
        status(QString("正在准备 Minecraft 资源 · %1 / %2").arg(offset).arg(batch.size()));
        downloadBatch(batch.mid(offset, batchSize));
    }
    if (metadata.value("virtual").toBool() || metadata.value("map_to_resources").toBool()) {
        const auto virtualRoot = metadata.value("map_to_resources").toBool()
            ? root + "/pack/.minecraft/resources" : assets + "/virtual/" + id;
        for (auto it = objects.begin(); it != objects.end(); ++it) {
            const auto name = it.key();
            if (name.contains('\\') || name.contains(':') || name.startsWith('/') ||
                name.split('/').contains("..")) fail("游戏资源路径无效。");
            const auto target = virtualRoot + "/" + name;
            const auto hash = it.value().toObject().value("hash").toString();
            const auto source = assets + "/objects/" + hash.left(2) + "/" + hash;
            if (!QFileInfo::exists(target)) {
                makeDirectory(QFileInfo(target).absolutePath());
                if (!QFile::copy(source, target)) fail("无法安装游戏资源 " + name);
            }
        }
    }
}

static void installGalaxyMods(const QString &root, const QJsonObject &mods)
{
    for (auto it = mods.begin(); it != mods.end(); ++it) {
        const auto target = root + "/pack/.minecraft/mods/" + it.key();
        const auto source = appPath() + "/galaxy-mods/" + it.key();
        const auto digest = it.value().toString().toLatin1();
        const bool installed = QFileInfo::exists(target);
        const auto bytes = readFile(installed ? target : source);
        if (QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() != digest)
            fail("Galaxy 客户端模组校验失败：" + it.key());
        if (installed) continue;
        QSaveFile output(target);
        if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit())
            fail("无法安装 Galaxy 模组：" + it.key());
    }
}

void prepareGame(const QString &root, const Progress &status)
{
    javaPath();
    const auto meta = metadata();
    makeDirectory(root);
    installPack(root, meta, status);
    installGalaxyMods(root, meta.value("galaxyMods").toObject());
    status("正在准备 Minecraft 和 Forge 游戏库…");
    QList<Download> downloads;
    const auto libraries = meta.value("libraries").toObject().value(platform).toArray();
    for (const auto &item : libraries) {
        const auto library = item.toObject();
        const auto path = root + "/" + library.value("path").toString();
        if (library.contains("url")) downloads.append(fileSpec(path, library));
        else if (!QFileInfo(path).isFile()) fail("缺少整合包启动库：" + path);
    }
    downloadBatch(downloads);
    installAssets(root, meta.value("assetIndex").toObject(), status);
}

void startGame(const QString &root, const Account &account, const Progress &status)
{
    const auto meta = metadata();
    const auto game = root + "/pack/.minecraft";
    const auto index = meta.value("assetIndex").toObject().value("id").toString();
    const auto assets = root + "/assets";
    const auto assetMetadata = readJson(readFile(assets + "/indexes/" + index + ".json"));
    const auto assetRoot = assetMetadata.value("virtual").toBool() ? assets + "/virtual/" + index : assets;

    QTemporaryDir natives(root + "/.natives-XXXXXX");
    if (!natives.isValid()) fail("无法建立本地库目录。");
    QStringList classpath;
    const auto libraries = meta.value("libraries").toObject().value(platform).toArray();
    for (const auto &item : libraries) {
        const auto library = item.toObject();
        const auto path = root + "/" + library.value("path").toString();
        if (library.contains("extract")) extract(path, natives.path(), "", strings(library.value("extract")));
        else classpath.append(QDir(game).relativeFilePath(path));
    }
    const QHash<QString, QString> values{
        {"auth_player_name", account.name}, {"auth_uuid", account.uuid},
        {"auth_access_token", account.accessToken}, {"user_properties", "{}"},
        {"user_type", "msa"}, {"version_name", "1.7.10"}, {"game_directory", game},
        {"assets_root", assetRoot}, {"assets_index_name", index}
    };
    auto arguments = strings(meta.value("jvm"));
    arguments << "-Xms1G" << "-Xmx6G" << "-Djava.library.path=" + natives.path()
              << "-cp" << classpath.join(classpathSeparator) << meta.value("main").toString();
    for (auto argument : strings(meta.value("arguments"))) {
        if (argument.startsWith("${") && argument.endsWith('}')) {
            const auto key = argument.mid(2, argument.size() - 3);
            if (!values.contains(key)) fail("不支持的游戏启动参数：" + key);
            argument = values.value(key);
        }
        arguments << argument;
    }
    for (const auto &tweaker : strings(meta.value("tweakers"))) arguments << "--tweakClass" << tweaker;
    arguments << "--server" << serverHost << "--port" << QString::number(serverPort);

    QFile log(root + "/game-console.log");
    if (!log.open(QIODevice::WriteOnly | QIODevice::Truncate)) fail("无法写入游戏日志。");
    QProcess process;
    process.setWorkingDirectory(game);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(javaPath(), arguments);
    if (!process.waitForStarted()) fail("Minecraft 进程无法启动：" + process.errorString());
    status("已启动 Minecraft，正在进入指定服务器…");
    const auto token = account.accessToken.toUtf8();
    auto writeLog = [&](QByteArray bytes) {
        log.write(bytes.replace(token, "<redacted>"));
    };
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(1000);
        while (process.canReadLine()) writeLog(process.readLine());
    }
    writeLog(process.readAll());
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        fail("Minecraft 异常退出。游戏日志：" + log.fileName());
}
