#include "auth.h"
#include "credentials.h"
#include "http.h"
#include <QDeadlineTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QThread>
#include <QUrlQuery>
#include <algorithm>

static const QString microsoft = "https://login.microsoftonline.com/consumers/oauth2/v2.0/";
static const QString scope = "XboxLive.SignIn XboxLive.offline_access";

static QJsonObject form(Http &http, const QString &path,
                        std::initializer_list<std::pair<QString, QString>> fields)
{
    QByteArray encoded;
    for (const auto &[key, value] : fields) {
        if (!encoded.isEmpty()) encoded += '&';
        encoded += QUrl::toPercentEncoding(key) + '=' + QUrl::toPercentEncoding(value);
    }
    return http.json(QUrl(microsoft + path), encoded, "application/x-www-form-urlencoded", {}, true);
}

static QString field(const QJsonObject &object, const QString &key)
{
    const auto value = object.value(key).toString();
    if (value.isEmpty()) fail("正版授权服务缺少必要字段：" + key);
    return value;
}

static QJsonObject deviceAuth(Http &http, const QString &clientId,
                              const std::function<void(QUrl, QString)> &authorize)
{
    const auto device = form(http, "devicecode", {{"client_id", clientId}, {"scope", scope}});
    if (device.contains("error"))
        fail("微软应用无法发起授权。请确认应用 ID、个人微软账号与公开客户端配置。");
    const auto deviceCode = field(device, "device_code");
    const QUrl url(field(device, "verification_uri"));
    if (url.scheme() != "https" || (url.host() != "www.microsoft.com" &&
        url.host() != "microsoft.com" && url.host() != "login.microsoftonline.com"))
        fail("微软返回的授权网址无效。");
    authorize(url, field(device, "user_code"));
    int interval = std::clamp(device.value("interval").toInt(5), 5, 60);
    const QDeadlineTimer deadline(std::clamp(device.value("expires_in").toInt(), 0, 1800) * 1000);
    while (!deadline.hasExpired() && !QThread::currentThread()->isInterruptionRequested()) {
        for (int second = 0; second < interval && !deadline.hasExpired(); ++second) {
            if (QThread::currentThread()->isInterruptionRequested()) fail("已取消正版授权。");
            QThread::sleep(1);
        }
        if (deadline.hasExpired()) break;
        auto result = form(http, "token", {{"client_id", clientId}, {"device_code", deviceCode},
                                          {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}});
        const auto error = result.value("error").toString();
        if (error.isEmpty()) return result;
        if (error == "slow_down") interval += 5;
        else if (error != "authorization_pending") fail("微软正版授权被拒绝、取消或已过期。");
    }
    fail("正版授权已取消或超时。");
}

static QByteArray encode(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

Account authenticate(const QString &clientId, const std::function<void(QUrl, QString)> &authorize)
{
    Http http;
    QJsonObject msa;
    const auto refreshToken = loadRefreshToken(clientId);
    if (!refreshToken.isEmpty()) {
        msa = form(http, "token", {{"client_id", clientId}, {"refresh_token", refreshToken},
                                    {"grant_type", "refresh_token"}, {"scope", scope}});
    }
    if (msa.value("access_token").toString().isEmpty()) msa = deviceAuth(http, clientId, authorize);

    const auto xbl = http.json(QUrl("https://user.auth.xboxlive.com/user/authenticate"), encode({
        {"Properties", QJsonObject{{"AuthMethod", "RPS"}, {"SiteName", "user.auth.xboxlive.com"},
                                   {"RpsTicket", "d=" + field(msa, "access_token")}}},
        {"RelyingParty", "http://auth.xboxlive.com"}, {"TokenType", "JWT"}
    }), "application/json");
    const auto xsts = http.json(QUrl("https://xsts.auth.xboxlive.com/xsts/authorize"), encode({
        {"Properties", QJsonObject{{"SandboxId", "RETAIL"}, {"UserTokens", QJsonArray{field(xbl, "Token")}}}},
        {"RelyingParty", "rp://api.minecraftservices.com/"}, {"TokenType", "JWT"}
    }), "application/json");
    const auto hashes = xsts.value("DisplayClaims").toObject().value("xui").toArray();
    const auto uhs = field(hashes.isEmpty() ? QJsonObject{} : hashes.first().toObject(), "uhs");

    const QString service = "https://api.minecraftservices.com";
    const auto minecraft = http.json(QUrl(service + "/authentication/login_with_xbox"),
        encode({{"identityToken", "XBL3.0 x=" + uhs + ";" + field(xsts, "Token")}}), "application/json");
    const auto access = field(minecraft, "access_token");
    const auto entitlements = http.json(QUrl(service + "/entitlements/mcstore"), {}, {}, access.toUtf8());
    bool ownsJava = false;
    for (const auto &item : entitlements.value("items").toArray()) {
        const auto name = item.toObject().value("name").toString();
        if (name == "game_minecraft" || name == "product_minecraft") ownsJava = true;
    }
    if (!ownsJava) fail("此微软账号没有可验证的 Minecraft Java 版游戏权益。");
    const auto profile = http.json(QUrl(service + "/minecraft/profile"), {}, {}, access.toUtf8());
    const auto name = field(profile, "name");
    const auto uuid = field(profile, "id");
    if (!QRegularExpression("^[A-Za-z0-9_]{1,16}$").match(name).hasMatch() ||
        !QRegularExpression("^[0-9a-fA-F]{32}$").match(uuid).hasMatch()) fail("Minecraft 个人资料无效。");

    const auto newRefreshToken = msa.value("refresh_token").toString();
    if (!newRefreshToken.isEmpty()) saveRefreshToken(clientId, newRefreshToken);
    return {name, uuid, access};
}
