#include "qrs/Content.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QUrlQuery>

namespace qrs {

static QString tr(const char *s) { return QCoreApplication::translate("qrs", s); }

static void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

static bool looksLikeEmail(const QString &s)
{
    static const QRegularExpression re(
        QStringLiteral(R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9](?:[A-Za-z0-9\-]*[A-Za-z0-9])?(?:\.[A-Za-z0-9](?:[A-Za-z0-9\-]*[A-Za-z0-9])?)+$)"));
    return s.size() <= 254 && re.match(s).hasMatch();
}

static QString normalizePhone(const QString &s)
{
    // Keep a leading '+', drop common separators; anything else is invalid.
    QString out;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (c.isDigit())
            out += c;
        else if (c == u'+' && out.isEmpty())
            out += c;
        else if (c == u' ' || c == u'-' || c == u'(' || c == u')' || c == u'.')
            continue;
        else
            return {};
    }
    const int digits = out.startsWith(u'+') ? out.size() - 1 : out.size();
    return digits >= 3 && digits <= 15 ? out : QString();
}

// ------------------------------------------------------------ classify ----

ContentInfo classify(const QString &text)
{
    ContentInfo info;
    info.label = tr("Text");
    const QString t = text.trimmed();
    if (t.isEmpty())
        return info;

    if (t.startsWith(QLatin1String("WIFI:"), Qt::CaseInsensitive)) {
        info.kind = ContentKind::Wifi;
        info.label = tr("Wi-Fi network");
        return info;
    }
    if (t.startsWith(QLatin1String("BEGIN:VCARD"), Qt::CaseInsensitive)) {
        info.kind = ContentKind::VCard;
        info.label = tr("Contact card");
        return info;
    }
    if (looksLikeEmail(t)) {
        info.kind = ContentKind::Email;
        info.label = tr("Email address");
        info.openUrl = QUrl(QStringLiteral("mailto:") + t);
        return info;
    }

    // Multi-line or space-containing text is never treated as a link.
    static const QRegularExpression whitespace(QStringLiteral(R"(\s)"));
    if (t.contains(whitespace) && !t.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive)
        && !t.startsWith(QLatin1String("sms"), Qt::CaseInsensitive))
        return info;

    static const QRegularExpression schemeRe(QStringLiteral(R"(^([A-Za-z][A-Za-z0-9+.\-]*):)"));
    const auto m = schemeRe.match(t);
    if (!m.hasMatch()) {
        // Bare "www.example.com" style links.
        if (t.startsWith(QLatin1String("www."), Qt::CaseInsensitive)) {
            const QUrl url(QStringLiteral("https://") + t, QUrl::StrictMode);
            if (url.isValid() && !url.host().isEmpty()) {
                info.kind = ContentKind::Url;
                info.label = tr("Web link");
                info.openUrl = url;
            }
        }
        return info;
    }

    const QString scheme = m.captured(1).toLower();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https")) {
        const QUrl url(t, QUrl::StrictMode);
        info.kind = ContentKind::Url;
        info.label = tr("Web link");
        if (url.isValid() && !url.host().isEmpty()) {
            info.openUrl = url;
            if (!url.userInfo().isEmpty())
                info.warning = tr("This link contains embedded credentials; the real destination is %1.")
                                   .arg(url.host());
            else if (url.host() != url.host(QUrl::FullyEncoded))
                info.warning = tr("This link uses international characters; its real host is %1.")
                                   .arg(url.host(QUrl::FullyEncoded));
            else if (scheme == QLatin1String("http"))
                info.warning = tr("This link is not encrypted (http).");
        } else {
            info.warning = tr("The link is malformed and cannot be opened.");
        }
    } else if (scheme == QLatin1String("mailto")) {
        info.kind = ContentKind::Email;
        info.label = tr("Email");
        const QUrl url(t);
        if (url.isValid())
            info.openUrl = url;
    } else if (scheme == QLatin1String("tel")) {
        info.kind = ContentKind::Phone;
        info.label = tr("Phone number");
        const QString number = normalizePhone(t.mid(4));
        if (!number.isEmpty())
            info.openUrl = QUrl(QStringLiteral("tel:") + number);
    } else if (scheme == QLatin1String("sms") || scheme == QLatin1String("smsto")) {
        info.kind = ContentKind::Sms;
        info.label = tr("Text message");
        const QString number = normalizePhone(t.section(u':', 1, 1).section(u'?', 0, 0));
        if (!number.isEmpty())
            info.openUrl = QUrl(QStringLiteral("sms:") + number);
    } else if (scheme == QLatin1String("geo")) {
        info.kind = ContentKind::Geo;
        info.label = tr("Location");
        static const QRegularExpression geoRe(QStringLiteral(R"(^geo:-?\d+(\.\d+)?,-?\d+(\.\d+)?([,;?].*)?$)"),
                                              QRegularExpression::CaseInsensitiveOption);
        if (geoRe.match(t).hasMatch())
            info.openUrl = QUrl(t);
    } else {
        info.kind = ContentKind::Url;
        info.label = tr("Link (%1)").arg(scheme);
        info.warning = tr("For safety, \"%1:\" links cannot be opened from QR Studio.").arg(scheme);
    }
    return info;
}

// ------------------------------------------------------------ builders ----

QString buildUrl(const QString &input, QString *error)
{
    QString s = input.trimmed();
    if (s.isEmpty()) {
        setError(error, tr("Enter a web address."));
        return {};
    }
    if (!s.contains(QLatin1String("://")))
        s.prepend(QLatin1String("https://"));
    const QUrl url(s, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() || !url.host().contains(u'.')
        || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
        setError(error, tr("That does not look like a valid http(s) web address."));
        return {};
    }
    return QString::fromLatin1(url.toEncoded());
}

QString buildEmail(const QString &address, const QString &subject, const QString &body, QString *error)
{
    const QString a = address.trimmed();
    if (!looksLikeEmail(a)) {
        setError(error, tr("Enter a valid email address."));
        return {};
    }
    QUrl url;
    url.setScheme(QStringLiteral("mailto"));
    url.setPath(a);
    QUrlQuery query;
    if (!subject.isEmpty())
        query.addQueryItem(QStringLiteral("subject"), QString::fromLatin1(QUrl::toPercentEncoding(subject)));
    if (!body.isEmpty())
        query.addQueryItem(QStringLiteral("body"), QString::fromLatin1(QUrl::toPercentEncoding(body)));
    if (!query.isEmpty())
        url.setQuery(query.toString(QUrl::FullyEncoded), QUrl::StrictMode);
    return QString::fromLatin1(url.toEncoded());
}

QString buildPhone(const QString &number, QString *error)
{
    const QString n = normalizePhone(number.trimmed());
    if (n.isEmpty()) {
        setError(error, tr("Enter a valid phone number (3-15 digits, optional leading +)."));
        return {};
    }
    return QStringLiteral("tel:") + n;
}

QString buildSms(const QString &number, const QString &message, QString *error)
{
    const QString n = normalizePhone(number.trimmed());
    if (n.isEmpty()) {
        setError(error, tr("Enter a valid phone number (3-15 digits, optional leading +)."));
        return {};
    }
    return QStringLiteral("SMSTO:") + n + u':' + message;
}

static QString wifiEscape(const QString &s)
{
    QString out;
    for (const QChar c : s) {
        if (c == u'\\' || c == u';' || c == u',' || c == u':' || c == u'"')
            out += u'\\';
        out += c;
    }
    return out;
}

QString buildWifi(const QString &ssid, const QString &password, WifiSecurity security, bool hidden,
                  QString *error)
{
    if (ssid.isEmpty()) {
        setError(error, tr("Enter the network name (SSID)."));
        return {};
    }
    if (ssid.toUtf8().size() > 32) {
        setError(error, tr("The network name is longer than 32 bytes."));
        return {};
    }
    if (security == WifiSecurity::Wpa && (password.size() < 8 || password.toUtf8().size() > 63)) {
        setError(error, tr("WPA passwords must be 8 to 63 characters long."));
        return {};
    }
    if (security == WifiSecurity::Wep && password.isEmpty()) {
        setError(error, tr("Enter the WEP key."));
        return {};
    }
    QString out = QStringLiteral("WIFI:T:");
    out += security == WifiSecurity::Wpa ? QStringLiteral("WPA")
           : security == WifiSecurity::Wep ? QStringLiteral("WEP") : QStringLiteral("nopass");
    out += QStringLiteral(";S:") + wifiEscape(ssid) + u';';
    if (security != WifiSecurity::Open)
        out += QStringLiteral("P:") + wifiEscape(password) + u';';
    if (hidden)
        out += QStringLiteral("H:true;");
    out += u';';
    return out;
}

WifiInfo parseWifi(const QString &text)
{
    WifiInfo info;
    const QString t = text.trimmed();
    if (!t.startsWith(QLatin1String("WIFI:"), Qt::CaseInsensitive))
        return info;

    // Split on unescaped ';' into KEY:value fields.
    QString key, value;
    bool inValue = false, escape = false;
    auto commit = [&] {
        const QString k = key.toUpper();
        if (k == QLatin1String("S")) info.ssid = value;
        else if (k == QLatin1String("P")) info.password = value;
        else if (k == QLatin1String("T")) info.security = value;
        else if (k == QLatin1String("H")) info.hidden = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
        key.clear();
        value.clear();
        inValue = false;
    };
    for (int i = 5; i < t.size(); ++i) {
        const QChar c = t[i];
        if (escape) {
            (inValue ? value : key) += c;
            escape = false;
        } else if (c == u'\\') {
            escape = true;
        } else if (c == u';') {
            commit();
        } else if (c == u':' && !inValue) {
            inValue = true;
        } else {
            (inValue ? value : key) += c;
        }
    }
    if (!key.isEmpty())
        commit();
    info.valid = !info.ssid.isEmpty();
    return info;
}

} // namespace qrs
