// QR payload helpers: build well-formed payloads and classify decoded text.
#pragma once

#include <QString>
#include <QUrl>

namespace qrs {

enum class ContentKind { Text, Url, Email, Phone, Sms, Wifi, Geo, VCard };

struct ContentInfo {
    ContentKind kind = ContentKind::Text;
    QString label;     // human-readable kind, e.g. "Web link"
    QUrl openUrl;      // valid only when the content may be offered for opening
    QString warning;   // e.g. link uses an unusual scheme
    bool canOpen() const { return openUrl.isValid(); }
};

// Classifies decoded text. Only http, https, mailto, tel, sms and geo targets
// are ever offered for opening; everything else (file:, javascript:, custom
// app schemes, ...) is reported as not openable. Opening always remains an
// explicit user action in the UI.
ContentInfo classify(const QString &text);

// --- Payload builders. Each returns an empty string and sets `error` on invalid input.
QString buildUrl(const QString &url, QString *error = nullptr);
QString buildEmail(const QString &address, const QString &subject, const QString &body,
                   QString *error = nullptr);
QString buildPhone(const QString &number, QString *error = nullptr);
QString buildSms(const QString &number, const QString &message, QString *error = nullptr);

enum class WifiSecurity { Open, Wep, Wpa };
QString buildWifi(const QString &ssid, const QString &password, WifiSecurity security,
                  bool hidden, QString *error = nullptr);

// Parsed form of a WIFI: payload (for display).
struct WifiInfo {
    bool valid = false;
    QString ssid, password, security;
    bool hidden = false;
};
WifiInfo parseWifi(const QString &text);

} // namespace qrs
