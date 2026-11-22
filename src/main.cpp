#include "auth.h"
#include "config.h"
#include "game.h"
#include "http.h"
#include <QApplication>
#include <QClipboard>
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
        setWindowTitle("GTNH 启动器");
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
        openBrowser.hide();
        cancel.hide();

        selected = settings.value("directory").toString();
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
        if (!selected.isEmpty()) QMetaObject::invokeMethod(this, [this] { launch(); }, Qt::QueuedConnection);
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
        choose.setEnabled(false);
        signIn.setEnabled(false);
        status.setText("正在准备游戏…");
        worker = QThread::create([this, location = selected, app] {
            auto report = [this](const QString &text) {
                QMetaObject::invokeMethod(this, [this, text] { status.setText(text); }, Qt::QueuedConnection);
            };
            try {
                const auto root = gameRoot(location);
                QLockFile lock(root + "/.launcher.lock");
                lockInstallation(lock);
                prepareGame(root, report);
                report("正在验证 Minecraft 正版账号…");
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
                report("已验证 " + account.name + "，正在启动游戏…");
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
    bool diagnostic = false;
    for (int i = 1; i < argc; ++i)
        diagnostic |= QByteArray(argv[i]) == "--prepare-only";
    QCoreApplication::setOrganizationName("GalaxyGTNH");
    QCoreApplication::setApplicationName("GTNH Launcher");
    QCoreApplication::setApplicationVersion("0.1.0");
    if (diagnostic) {
        QCoreApplication app(argc, argv);
        QCommandLineParser parser;
        parser.addHelpOption();
        const QCommandLineOption prepare("prepare-only", "Install without authentication or game launch.", "directory");
        parser.addOption(prepare);
        parser.process(app);
        try {
            const auto root = gameRoot(parser.value(prepare));
            QLockFile lock(root + "/.launcher.lock");
            lockInstallation(lock);
            prepareGame(root, [](const QString &text) { qInfo().noquote() << text; });
            qInfo().noquote() << "GTNH ready in" << root;
            return 0;
        } catch (const std::exception &error) {
            qCritical().noquote() << error.what();
            return 1;
        }
    }
    QApplication app(argc, argv);
    Launcher window;
    window.show();
    return app.exec();
}
