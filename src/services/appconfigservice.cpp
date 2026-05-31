// =============================================================================
// @file    appconfigservice.cpp
// @brief   统一配置文件存取服务实现
// =============================================================================
#include "services/appconfigservice.h"

#include "tabs/configtab.h"
#include "tabs/flashstoragetab.h"
#include "tabs/fwflashtab.h"
#include "tabs/oscilloscoptab.h"
#include "tabs/registerrwtab.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

AppConfigService::AppConfigService(ConfigTab *configTab,
                                   RegisterRwTab *registerTab,
                                   OscilloscopTab *scopeTab,
                                   FwFlashTab *fwFlashTab,
                                   FlashStorageTab *flashStorageTab,
                                   QObject *parent)
    : QObject(parent)
    , m_configTab(configTab)
    , m_registerTab(registerTab)
    , m_scopeTab(scopeTab)
    , m_fwFlashTab(fwFlashTab)
    , m_flashStorageTab(flashStorageTab) {}

// -----------------------------------------------------------------------------
// 写：采集各页面 → JSON 文件
// -----------------------------------------------------------------------------
bool AppConfigService::write(const QString &path, QString *errorMessage) {
    QJsonObject root;
    root.insert(QStringLiteral("version"), kConfigVersion);
    if (m_configTab != nullptr)
        root.insert(QStringLiteral("device"), m_configTab->collectDeviceConfig());
    if (m_registerTab != nullptr)
        root.insert(QStringLiteral("registers"), m_registerTab->collectRegisterRows());
    if (m_scopeTab != nullptr)
        root.insert(QStringLiteral("scope"), m_scopeTab->collectScopeConfig());
    if (m_fwFlashTab != nullptr)
        root.insert(QStringLiteral("flash"), m_fwFlashTab->collectFlashConfig());
    if (m_flashStorageTab != nullptr)
        root.insert(QStringLiteral("flashStore"), m_flashStorageTab->collectFlashStoreConfig());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("无法写入文件：%1").arg(file.errorString());
        return false;
    }
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("写入文件失败：%1").arg(file.errorString());
        return false;
    }
    file.close();
    return true;
}

// -----------------------------------------------------------------------------
// 读：JSON 文件 → 回填各页面（缺段跳过 + 日志，不整体失败）
// -----------------------------------------------------------------------------
bool AppConfigService::read(const QString &path, QString *errorMessage) {
    QFile file(path);
    if (!file.exists()) {
        if (errorMessage != nullptr) *errorMessage = QStringLiteral("文件不存在：%1").arg(path);
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("无法读取文件：%1").arg(file.errorString());
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage != nullptr)
            *errorMessage = QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt(0);
    if (version != kConfigVersion) {
        // 不阻断：仅告警，尽力按当前版本字段恢复
        emit logMessage(QStringLiteral("[配置] 版本不一致（文件 v%1, 当前 v%2），尝试尽力恢复")
                            .arg(version).arg(kConfigVersion));
    }

    auto applySection = [&](const char *key, auto applyFn) {
        const QString k = QString::fromLatin1(key);
        if (root.contains(k)) {
            applyFn();
        } else {
            emit logMessage(QStringLiteral("[配置] 缺少 %1 段，跳过").arg(k));
        }
    };

    if (m_configTab != nullptr)
        applySection("device", [&] {
            m_configTab->applyDeviceConfig(root.value(QStringLiteral("device")).toObject());
        });
    if (m_registerTab != nullptr)
        applySection("registers", [&] {
            m_registerTab->applyRegisterRows(root.value(QStringLiteral("registers")).toArray());
        });
    if (m_scopeTab != nullptr)
        applySection("scope", [&] {
            m_scopeTab->applyScopeConfig(root.value(QStringLiteral("scope")).toObject());
        });
    if (m_fwFlashTab != nullptr)
        applySection("flash", [&] {
            m_fwFlashTab->applyFlashConfig(root.value(QStringLiteral("flash")).toObject());
        });
    if (m_flashStorageTab != nullptr)
        applySection("flashStore", [&] {
            m_flashStorageTab->applyFlashStoreConfig(
                root.value(QStringLiteral("flashStore")).toObject());
        });

    return true;
}

// -----------------------------------------------------------------------------
// 槽：来自 ConfigTab 的 Write / Read 请求
// -----------------------------------------------------------------------------
void AppConfigService::onWriteRequested(const QString &path) {
    QString err;
    if (write(path, &err)) {
        emit logMessage(QStringLiteral("[配置] 已写入：%1").arg(path));
        emit finished(true, path);
    } else {
        emit logMessage(QStringLiteral("[配置] 写入失败：%1").arg(err));
        emit finished(false, err);
    }
}

void AppConfigService::onReadRequested(const QString &path) {
    QString err;
    if (read(path, &err)) {
        emit logMessage(QStringLiteral("[配置] 已读取并回填：%1").arg(path));
        emit finished(true, path);
    } else {
        emit logMessage(QStringLiteral("[配置] 读取失败：%1").arg(err));
        emit finished(false, err);
    }
}
