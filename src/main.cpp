#include "auth.h"
#include "config.h"
#include "game.h"
#include "http.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCommandLineParser>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QLockFile>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <cstdio>

static QString clientId()
{
    return qEnvironmentVariable("GTNH_MSA_CLIENT_ID", QString::fromLatin1(defaultClientId)).trimmed();
}

static QString gameRoot(const QString &selected)
{
    const auto directory = QDir(selected).canonicalPath();
    if (directory.isEmpty()) fail("所选目录不存在。");
    const auto root = QDir(directory).filePath("GTNH-Launcher-2.9.0");
    if (QFileInfo(root).isSymLink()) fail("安装目录不能是符号链接。");
    makeDirectory(root);
    return root;
}

static void lockInstallation(QLockFile &lock)
{
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) fail("无法锁定安装目录。请检查写入权限，或关闭其他启动器。");
}

class Launcher : public QWidget {
public:
    Launcher()
    {
        setWindowTitle("Galaxy GTNH Launcher");
        resize(535, 265);
        auto layout = new QVBoxLayout(this);
        auto title = new QLabel("GT New Horizons 2.9.0");
        auto font = title->font();
        font.setPointSize(22);
        font.setBold(true);
        title->setFont(font);
        layout->addWidget(title);
        auto server = new QLabel(QString("服务器 %1").arg(serverHost));
        layout->addWidget(server);
        auto sourceRow = new QHBoxLayout;
        auto sourceLabel = new QLabel("Minecraft 下载源");
        sourceLabel->setBuddy(&downloadSource);
        sourceRow->addWidget(sourceLabel);
        downloadSource.addItem("Mojang 官方源", "mojang");
        downloadSource.addItem("BMCLAPI 国内社区镜像", "bmclapi");
        sourceRow->addWidget(&downloadSource, 1);
        layout->addLayout(sourceRow);
        layout->addStretch();
        layout->addWidget(&status);
        status.setWordWrap(true);
        status.setTextFormat(Qt::PlainText);
        layout->addWidget(&directory);
        directory.setWordWrap(true);
        directory.setTextFormat(Qt::PlainText);
        auto buttons = new QHBoxLayout;
        buttons->addWidget(&choose);
        buttons->addWidget(&signIn);
        buttons->addWidget(&openBrowser);
        buttons->addWidget(&cancel);
        layout->addLayout(buttons);
        auto notice = new QLabel("非 Minecraft 官方产品。未经 Mojang 或 Microsoft 批准，与其无关联。");
        notice->setWordWrap(true);
        layout->addWidget(notice);
        openBrowser.hide();
        cancel.hide();

        selected = settings.value("directory").toString();
        const auto savedSource = downloadSource.findData(settings.value("downloadSource", "mojang"));
        if (savedSource >= 0) downloadSource.setCurrentIndex(savedSource);
        connect(&downloadSource, &QComboBox::currentIndexChanged, this, [this] {
            settings.setValue("downloadSource", downloadSource.currentData());
        });
        showDirectory();
        connect(&choose, &QPushButton::clicked, this, [this] {
            const auto path = QFileDialog::getExistingDirectory(this, "选择 GTNH 安装目录", selected);
            if (path.isEmpty()) return;
            selected = path;
            settings.setValue("directory", path);
            showDirectory();
            launch();
        });
        connect(&signIn, &QPushButton::clicked, this, [this] {
            if (selected.isEmpty()) choose.click();
            else launch();
        });
        connect(&openBrowser, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(verificationCode);
            QDesktopServices::openUrl(verificationUrl);
        });
        connect(&cancel, &QPushButton::clicked, this, [this] {
            if (worker) worker->requestInterruption();
            cancel.setEnabled(false);
        });
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (!worker) {
            event->accept();
            return;
        }
        if (cancel.isVisible()) cancel.click();
        event->ignore();
    }

private:
    void showDirectory()
    {
        directory.setText(selected.isEmpty() ? "首次启动请选择安装目录。" : selected);
    }

    void launch()
    {
        if (worker) return;
        const auto app = clientId();
        if (!QRegularExpression("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$").match(app).hasMatch()) {
            status.setText("此发行版尚未配置可用的微软正版登录应用 ID。");
            return;
        }
        const auto source = downloadSource.currentData().toString() == "bmclapi"
            ? DownloadSource::Bmclapi : DownloadSource::Mojang;
        choose.setEnabled(false);
        signIn.setEnabled(false);
        downloadSource.setEnabled(false);
        status.setText("正在验证 Minecraft 正版账号…");
        worker = QThread::create([this, location = selected, app, source] {
            auto report = [this](const QString &text) {
                QMetaObject::invokeMethod(this, [this, text] { status.setText(text); }, Qt::QueuedConnection);
            };
            try {
                const auto root = gameRoot(location);
                QLockFile lock(root + "/.launcher.lock");
                lockInstallation(lock);
                const auto account = authenticate(app, [this](const QUrl &url, const QString &code) {
                    QMetaObject::invokeMethod(this, [this, url, code] {
                        verificationUrl = url;
                        verificationCode = code;
                        status.setText("请在微软登录页输入代码 " + code + "，并授权你的正版账号。");
                        openBrowser.show();
                        cancel.setEnabled(true);
                        cancel.show();
                        QDesktopServices::openUrl(url);
                    }, Qt::QueuedConnection);
                });
                if (QThread::currentThread()->isInterruptionRequested()) fail("已取消正版授权。");
                QMetaObject::invokeMethod(this, [this] { hideAuth(); }, Qt::QueuedConnection);
                report("已验证 " + account.name + "，正在准备游戏…");
                prepareGame(root, source, report);
                report("正在启动游戏…");
                startGame(root, account, report);
                report("游戏已关闭。");
            } catch (const std::exception &error) {
                report(QString::fromUtf8(error.what()));
            }
        });
        connect(worker, &QThread::finished, this, [this] {
            worker->deleteLater();
            worker = nullptr;
            hideAuth();
            choose.setEnabled(true);
            signIn.setEnabled(true);
            downloadSource.setEnabled(true);
        });
        worker->start();
    }

    void hideAuth()
    {
        openBrowser.hide();
        cancel.hide();
        verificationCode.clear();
        verificationUrl.clear();
    }

    QLabel status;
    QLabel directory;
    QComboBox downloadSource;
    QPushButton choose{"选择目录"};
    QPushButton signIn{"启动游戏"};
    QPushButton openBrowser{"复制代码并打开浏览器"};
    QPushButton cancel{"取消授权"};
    QSettings settings;
    QString selected;
    QThread *worker = nullptr;
    QUrl verificationUrl;
    QString verificationCode;
};

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("GalaxyGTNH");
    QCoreApplication::setApplicationName("GTNH Launcher");
    QCoreApplication::setApplicationVersion("0.1.0");
#if GTNH_ENABLE_INSTALL_DIAGNOSTICS
    bool diagnostic = false;
    for (int i = 1; i < argc; ++i)
        diagnostic |= QByteArray(argv[i]) == "--prepare-only";
    if (diagnostic) {
        QCoreApplication app(argc, argv);
        QCommandLineParser parser;
        parser.addHelpOption();
        const QCommandLineOption prepare("prepare-only", "Install without authentication or game launch.", "directory");
        const QCommandLineOption download("download-source", "Minecraft download source: mojang or bmclapi.",
                                          "source", "mojang");
        parser.addOption(prepare);
        parser.addOption(download);
        parser.process(app);
        try {
            const auto sourceName = parser.value(download);
            if (sourceName != "mojang" && sourceName != "bmclapi") fail("未知 Minecraft 下载源。");
            const auto source = sourceName == "bmclapi" ? DownloadSource::Bmclapi : DownloadSource::Mojang;
            const auto root = gameRoot(parser.value(prepare));
            QLockFile lock(root + "/.launcher.lock");
            lockInstallation(lock);
            prepareGame(root, source, [](const QString &text) { qInfo().noquote() << text; });
            qInfo().noquote() << "GTNH ready in" << root;
            return 0;
        } catch (const std::exception &error) {
            qCritical().noquote() << error.what();
            return 1;
        }
    }
#endif
    QApplication app(argc, argv);
    app.setApplicationDisplayName("Galaxy GTNH Launcher");
    Launcher window;
    window.show();
    return app.exec();
}
