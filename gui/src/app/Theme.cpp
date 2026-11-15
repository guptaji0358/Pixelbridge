#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPolygonF>
#include <QStandardPaths>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace app {

static ThemeColors g_current;

bool systemPrefersDark()
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

ThemeColors themeColors(ThemeMode mode)
{
    const bool dark = mode == ThemeMode::Dark || (mode == ThemeMode::System && systemPrefersDark());
    ThemeColors c;
    c.dark = dark;
    if (dark) {
        c.window = "#15171c";      c.surface = "#1e2128";     c.surfaceAlt = "#262a33";
        c.border = "#343945";      c.text = "#e8eaf0";        c.textMuted = "#9aa1b2";
        c.accent = "#5b8def";      c.accentHover = "#74a0f5"; c.accentText = "#0c1220";
        c.danger = "#f0707a";      c.success = "#4cc38a";     c.warning = "#e5b454";
        c.selection = "#2c3f66";
    } else {
        c.window = "#f4f5f8";      c.surface = "#ffffff";     c.surfaceAlt = "#eef0f5";
        c.border = "#d9dce4";      c.text = "#1c2030";        c.textMuted = "#667085";
        c.accent = "#2f6fed";      c.accentHover = "#1f5cd6"; c.accentText = "#ffffff";
        c.danger = "#d23c4b";      c.success = "#1f9d61";     c.warning = "#b7791f";
        c.selection = "#dbe6fd";
    }
    return c;
}

const ThemeColors &currentTheme() { return g_current; }

// Style sheets can only draw arrows and check marks from image files, so the
// few glyphs needed are painted once per theme into the user's cache folder.
static QString glyphDir(const ThemeColors &c)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + (c.dark ? QStringLiteral("/glyphs-dark") : QStringLiteral("/glyphs-light"));
    QDir().mkpath(dir);
    auto save = [&](const QString &name, const QColor &color, const QPolygonF &shape, bool stroke) {
        QImage img(32, 32, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        if (stroke) {
            p.setPen(QPen(color, 4.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(shape);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPolygon(shape);
        }
        p.end();
        img.save(dir + u'/' + name);
    };
    save(QStringLiteral("up.png"), c.textMuted, QPolygonF({{8, 21}, {16, 11}, {24, 21}}), true);
    save(QStringLiteral("down.png"), c.textMuted, QPolygonF({{8, 11}, {16, 21}, {24, 11}}), true);
    save(QStringLiteral("check.png"), c.accentText, QPolygonF({{7, 17}, {13, 23}, {25, 9}}), true);
    return dir;
}

static QString styleSheet(const ThemeColors &c)
{
    QString qss = QStringLiteral(R"(
* { font-family: "Segoe UI Variable Text", "Segoe UI", sans-serif; font-size: 10pt; }
QMainWindow, QWidget#page { background: @window; }
QLabel { color: @text; background: transparent; }
QLabel[role="title"] { font-size: 17pt; font-weight: 600; }
QLabel[role="subtitle"], QLabel[role="muted"] { color: @muted; }
QLabel[role="section"] { font-weight: 600; color: @muted; font-size: 9pt; }
QLabel[role="warning"] { color: @warning; }
QLabel[role="error"] { color: @danger; }
QLabel[role="success"] { color: @success; font-weight: 600; }

QFrame#card { background: @surface; border: 1px solid @border; border-radius: 10px; }
QFrame#sidebar { background: @surface; border: none; border-right: 1px solid @border; }
QLabel#brand { font-size: 13pt; font-weight: 700; padding: 4px 6px 12px 6px; }

QListWidget#nav { background: transparent; border: none; outline: 0; }
QListWidget#nav::item { color: @text; padding: 9px 12px; margin: 2px 0; border-radius: 7px; }
QListWidget#nav::item:hover { background: @surfaceAlt; }
QListWidget#nav::item:selected { background: @selection; color: @text; font-weight: 600; }

QPushButton { background: @surface; color: @text; border: 1px solid @border; border-radius: 7px;
              padding: 7px 14px; min-height: 18px; }
QPushButton:hover { background: @surfaceAlt; }
QPushButton:pressed { background: @border; }
QPushButton:disabled { color: @muted; background: @surfaceAlt; border-color: @surfaceAlt; }
QPushButton:focus { border-color: @accent; }
QPushButton[kind="primary"] { background: @accent; color: @accentText; border-color: @accent; font-weight: 600; }
QPushButton[kind="primary"]:hover { background: @accentHover; border-color: @accentHover; }
QPushButton[kind="primary"]:disabled { background: @surfaceAlt; color: @muted; border-color: @surfaceAlt; }
QPushButton[kind="danger"] { color: @danger; }
QPushButton[kind="danger"]:disabled { color: @muted; }

QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QComboBox {
    background: @surface; color: @text; border: 1px solid @border; border-radius: 7px;
    padding: 6px 8px; selection-background-color: @accent; selection-color: @accentText; }
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: @accent; }
QLineEdit:disabled, QPlainTextEdit:disabled, QSpinBox:disabled, QComboBox:disabled { color: @muted; background: @surfaceAlt; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: @surface; color: @text; border: 1px solid @border;
                              selection-background-color: @selection; selection-color: @text; }
QComboBox::down-arrow { image: url(@glyphs/down.png); width: 12px; height: 12px; }
QSpinBox { padding-right: 24px; }
QSpinBox::up-button, QSpinBox::down-button { subcontrol-origin: border; width: 22px; border: none;
    background: transparent; }
QSpinBox::up-button { subcontrol-position: top right; margin-top: 3px; }
QSpinBox::down-button { subcontrol-position: bottom right; margin-bottom: 3px; }
QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: @surfaceAlt; border-radius: 4px; }
QSpinBox::up-arrow { image: url(@glyphs/up.png); width: 10px; height: 10px; }
QSpinBox::down-arrow { image: url(@glyphs/down.png); width: 10px; height: 10px; }
QCheckBox, QRadioButton { color: @text; spacing: 7px; background: transparent; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid @muted; border-radius: 4px; background: @surface; }
QCheckBox::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; image: url(@glyphs/check.png); }
QCheckBox::indicator:disabled { border-color: @border; background: @surfaceAlt; }

QTableView, QTreeView, QListView { background: @surface; alternate-background-color: @surfaceAlt; color: @text;
    border: 1px solid @border; border-radius: 8px; gridline-color: transparent;
    selection-background-color: @selection; selection-color: @text; outline: 0; }
QTableView::item { padding: 4px 6px; }
QHeaderView::section { background: @surfaceAlt; color: @muted; border: none; border-bottom: 1px solid @border;
    padding: 7px 8px; font-weight: 600; }
QTableCornerButton::section { background: @surfaceAlt; border: none; }

QProgressBar { background: @surfaceAlt; border: none; border-radius: 5px; height: 10px; text-align: center; color: transparent; }
QProgressBar::chunk { background: @accent; border-radius: 5px; }

QScrollBar:vertical { background: transparent; width: 11px; margin: 2px; }
QScrollBar::handle:vertical { background: @border; border-radius: 4px; min-height: 28px; }
QScrollBar:horizontal { background: transparent; height: 11px; margin: 2px; }
QScrollBar::handle:horizontal { background: @border; border-radius: 4px; min-width: 28px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QTabWidget::pane { border: none; }
QTabBar::tab { background: transparent; color: @muted; padding: 7px 14px; border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: @text; border-bottom-color: @accent; font-weight: 600; }
QToolTip { background: @surface; color: @text; border: 1px solid @border; padding: 4px 6px; }
QStatusBar { background: @surface; color: @muted; border-top: 1px solid @border; }
QStatusBar QLabel { color: @muted; }
QMenu { background: @surface; color: @text; border: 1px solid @border; padding: 4px; }
QMenu::item { padding: 6px 22px; border-radius: 5px; }
QMenu::item:selected { background: @selection; }
QMenu::item:disabled { color: @muted; }
QMenu::separator { height: 1px; background: @border; margin: 4px 8px; }
QScrollArea { background: transparent; border: none; }
QSplitter::handle { background: transparent; }
)");
    qss.replace(QStringLiteral("@glyphs"), glyphDir(c));
    const QList<QPair<QString, QColor>> tokens = {
        {"@surfaceAlt", c.surfaceAlt}, {"@surface", c.surface},
        {"@window", c.window},    {"@border", c.border},         {"@muted", c.textMuted},
        {"@text", c.text},        {"@accentHover", c.accentHover}, {"@accentText", c.accentText},
        {"@accent", c.accent},    {"@danger", c.danger},         {"@success", c.success},
        {"@warning", c.warning},  {"@selection", c.selection}};
    // Longer token names are listed before their prefixes so they are replaced first.
    for (const auto &t : tokens)
        qss.replace(t.first, t.second.name());
    return qss;
}

void applyTheme(QApplication &application, ThemeMode mode)
{
    const ThemeColors c = themeColors(mode);
    g_current = c;
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.surface);
    p.setColor(QPalette::AlternateBase, c.surfaceAlt);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.surface);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::ToolTipBase, c.surface);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::PlaceholderText, c.textMuted);
    p.setColor(QPalette::Disabled, QPalette::Text, c.textMuted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.textMuted);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.textMuted);
    application.setPalette(p);
    application.setStyleSheet(styleSheet(c));
}

} // namespace app
