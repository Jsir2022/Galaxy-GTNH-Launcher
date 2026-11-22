#include "credentials.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>

static QString target(const QString &clientId)
{
    return "GTNH Launcher/Microsoft/" + clientId;
}

QString loadRefreshToken(const QString &clientId)
{
    const auto name = target(clientId).toStdWString();
    PCREDENTIALW entry = nullptr;
    if (!CredReadW(name.c_str(), CRED_TYPE_GENERIC, 0, &entry)) return {};
    auto token = QString::fromUtf8(reinterpret_cast<const char *>(entry->CredentialBlob), entry->CredentialBlobSize);
    CredFree(entry);
    return token;
}

void saveRefreshToken(const QString &clientId, const QString &token)
{
    auto name = target(clientId).toStdWString();
    auto bytes = token.toUtf8();
    CREDENTIALW entry{};
    entry.Type = CRED_TYPE_GENERIC;
    entry.TargetName = name.data();
    entry.Persist = CRED_PERSIST_LOCAL_MACHINE;
    entry.CredentialBlob = reinterpret_cast<LPBYTE>(bytes.data());
    entry.CredentialBlobSize = DWORD(bytes.size());
    CredWriteW(&entry, 0);
}

#else
#include <QLibrary>

// Load Secret Service support when the desktop provides it.
struct SecretStore {
    QLibrary library{QStringLiteral("secret-1"), 0};
    using Schema = void;
    Schema *(*create)(const char *, int, ...) = nullptr;
    void (*release)(Schema *) = nullptr;
    char *(*lookup)(const Schema *, void *, void **, ...) = nullptr;
    int (*store)(const Schema *, const char *, const char *, const char *, void *, void **, ...) = nullptr;
    void (*freePassword)(char *) = nullptr;
    Schema *schema = nullptr;

    SecretStore()
    {
        create = reinterpret_cast<decltype(create)>(library.resolve("secret_schema_new"));
        release = reinterpret_cast<decltype(release)>(library.resolve("secret_schema_unref"));
        lookup = reinterpret_cast<decltype(lookup)>(library.resolve("secret_password_lookup_sync"));
        store = reinterpret_cast<decltype(store)>(library.resolve("secret_password_store_sync"));
        freePassword = reinterpret_cast<decltype(freePassword)>(library.resolve("secret_password_free"));
        if (create && release && lookup && store && freePassword)
            schema = create("tech.jsir.gtnh-launcher", 0, "client-id", 0, nullptr);
    }

    ~SecretStore()
    {
        if (schema) release(schema);
    }
};

QString loadRefreshToken(const QString &clientId)
{
    SecretStore secrets;
    if (!secrets.schema) return {};
    auto password = secrets.lookup(secrets.schema, nullptr, nullptr, "client-id",
                                  clientId.toUtf8().constData(), nullptr);
    const auto token = QString::fromUtf8(password);
    if (password) secrets.freePassword(password);
    return token;
}

void saveRefreshToken(const QString &clientId, const QString &token)
{
    SecretStore secrets;
    if (secrets.schema)
        secrets.store(secrets.schema, nullptr, "GTNH Launcher Microsoft Account",
                      token.toUtf8().constData(), nullptr, nullptr,
                      "client-id", clientId.toUtf8().constData(), nullptr);
}
#endif
