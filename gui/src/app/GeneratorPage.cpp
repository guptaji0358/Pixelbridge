// GeneratorPage: widget construction. Behaviour lives in GeneratorPageActions.cpp.
#include "GeneratorPage.h"

#include "Widgets.h"

#include <QCheckBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace app {

static QFormLayout *formPage(QStackedWidget *stack)
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft);
    stack->addWidget(page);
    return form;
}

GeneratorPage::GeneratorPage(Settings *settings, QWidget *parent) : QWidget(parent), m_settings(settings)
{
    setObjectName("page");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);
    root->addWidget(makeHeader(tr("QR Generator"),
                               tr("Type your content, adjust the look, then save or copy the code. "
                                  "The preview updates as you type.")));

    auto *body = new QHBoxLayout;
    body->setSpacing(16);
    root->addLayout(body, 1);

    // ---- left column: content + options (scrolls on small windows) ---------
    auto *leftHost = new QWidget;
    auto *left = new QVBoxLayout(leftHost);
    left->setContentsMargins(0, 0, 6, 0);
    left->setSpacing(14);
    left->addWidget(buildForms());
    left->addWidget(buildOptions());
    left->addStretch();
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(leftHost);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(430);
    leftHost->setObjectName("page");
    body->addWidget(scroll, 4);

    // ---- right column: preview --------------------------------------------
    QVBoxLayout *card = nullptr;
    QFrame *frame = makeCard(&card);
    card->addWidget(makeLabel(tr("PREVIEW"), "section"));
    m_preview = new ImageView;
    m_preview->setPlaceholder(tr("Your QR code will appear here"), tr("Start typing on the left"));
    card->addWidget(m_preview, 1);
    m_message = makeLabel(QString(), "muted");
    m_message->setWordWrap(true);
    card->addWidget(m_message);
    m_info = makeLabel(QString(), "muted");
    card->addWidget(m_info);

    auto *row1 = new QHBoxLayout;
    m_saveButton = makeButton(tr("Save…"), "primary");
    m_copyButton = makeButton(tr("Copy image"));
    m_copyTextButton = makeButton(tr("Copy text"));
    m_copyTextButton->setToolTip(tr("Copy the exact text encoded in the code"));
    m_revealButton = makeButton(tr("Save && show in folder"));
    m_revealButton->setToolTip(tr("Save the image and open its folder so you can share the file"));
    row1->addWidget(m_saveButton);
    row1->addWidget(m_copyButton);
    row1->addWidget(m_copyTextButton);
    card->addLayout(row1);
    card->addWidget(m_revealButton);
    body->addWidget(frame, 4);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(120);
    connect(&m_debounce, &QTimer::timeout, this, &GeneratorPage::regenerate);
    connect(m_saveButton, &QPushButton::clicked, this, &GeneratorPage::save);
    connect(m_copyButton, &QPushButton::clicked, this, &GeneratorPage::copyImage);
    connect(m_revealButton, &QPushButton::clicked, this, &GeneratorPage::saveAndReveal);
    connect(m_copyTextButton, &QPushButton::clicked, this, [this] {
        if (!m_payload.isEmpty()) {
            QGuiApplication::clipboard()->setText(m_payload);
            emit statusMessage(tr("Encoded text copied to clipboard"));
        }
    });

    applyDefaults();
    regenerate();
}

QWidget *GeneratorPage::buildForms()
{
    QVBoxLayout *card = nullptr;
    QFrame *frame = makeCard(&card);
    card->addWidget(makeLabel(tr("CONTENT"), "section"));

    m_type = new QComboBox;
    m_type->addItems({tr("Text"), tr("Web link (URL)"), tr("Email"), tr("Phone number"),
                      tr("Text message (SMS)"), tr("Wi-Fi network")});
    card->addWidget(m_type);

    m_forms = new QStackedWidget;
    card->addWidget(m_forms);
    auto changed = [this] { scheduleUpdate(); };

    // 0: text
    m_plainText = new QPlainTextEdit;
    m_plainText->setPlaceholderText(tr("Any text, up to about 2,900 characters"));
    m_plainText->setMinimumHeight(130);
    m_forms->addWidget(m_plainText);
    connect(m_plainText, &QPlainTextEdit::textChanged, this, changed);

    // 1: url
    QFormLayout *url = formPage(m_forms);
    m_url = new QLineEdit;
    m_url->setPlaceholderText(QStringLiteral("https://example.com"));
    url->addRow(tr("Address"), m_url);
    connect(m_url, &QLineEdit::textChanged, this, changed);

    // 2: email
    QFormLayout *email = formPage(m_forms);
    m_emailTo = new QLineEdit;
    m_emailTo->setPlaceholderText(QStringLiteral("name@example.com"));
    m_emailSubject = new QLineEdit;
    m_emailBody = new QPlainTextEdit;
    m_emailBody->setMaximumHeight(90);
    email->addRow(tr("To"), m_emailTo);
    email->addRow(tr("Subject"), m_emailSubject);
    email->addRow(tr("Message"), m_emailBody);
    connect(m_emailTo, &QLineEdit::textChanged, this, changed);
    connect(m_emailSubject, &QLineEdit::textChanged, this, changed);
    connect(m_emailBody, &QPlainTextEdit::textChanged, this, changed);

    // 3: phone
    QFormLayout *phone = formPage(m_forms);
    m_phone = new QLineEdit;
    m_phone->setPlaceholderText(QStringLiteral("+1 555 123 4567"));
    phone->addRow(tr("Number"), m_phone);
    connect(m_phone, &QLineEdit::textChanged, this, changed);

    // 4: sms
    QFormLayout *sms = formPage(m_forms);
    m_smsNumber = new QLineEdit;
    m_smsNumber->setPlaceholderText(QStringLiteral("+1 555 123 4567"));
    m_smsMessage = new QPlainTextEdit;
    m_smsMessage->setMaximumHeight(90);
    sms->addRow(tr("Number"), m_smsNumber);
    sms->addRow(tr("Message"), m_smsMessage);
    connect(m_smsNumber, &QLineEdit::textChanged, this, changed);
    connect(m_smsMessage, &QPlainTextEdit::textChanged, this, changed);

    // 5: wifi
    QFormLayout *wifi = formPage(m_forms);
    m_wifiSsid = new QLineEdit;
    m_wifiPassword = new QLineEdit;
    m_wifiPassword->setEchoMode(QLineEdit::Password);
    auto *show = new QCheckBox(tr("Show password"));
    m_wifiSecurity = new QComboBox;
    m_wifiSecurity->addItems({tr("WPA / WPA2 / WPA3"), tr("WEP"), tr("None (open network)")});
    m_wifiHidden = new QCheckBox(tr("Hidden network"));
    wifi->addRow(tr("Network name"), m_wifiSsid);
    wifi->addRow(tr("Security"), m_wifiSecurity);
    wifi->addRow(tr("Password"), m_wifiPassword);
    wifi->addRow(QString(), show);
    wifi->addRow(QString(), m_wifiHidden);
    connect(show, &QCheckBox::toggled, this, [this](bool on) {
        m_wifiPassword->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(m_wifiSsid, &QLineEdit::textChanged, this, changed);
    connect(m_wifiPassword, &QLineEdit::textChanged, this, changed);
    connect(m_wifiHidden, &QCheckBox::toggled, this, changed);
    connect(m_wifiSecurity, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_wifiPassword->setEnabled(index != 2);
        scheduleUpdate();
    });

    connect(m_type, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_forms->setCurrentIndex(index);
        // Let the stacked widget shrink to the active form.
        for (int i = 0; i < m_forms->count(); ++i)
            m_forms->widget(i)->setSizePolicy(QSizePolicy::Preferred,
                                              i == index ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        m_forms->adjustSize();
        scheduleUpdate();
    });
    return frame;
}

QWidget *GeneratorPage::buildOptions()
{
    QVBoxLayout *card = nullptr;
    QFrame *frame = makeCard(&card);
    card->addWidget(makeLabel(tr("APPEARANCE"), "section"));

    auto *form = new QFormLayout;
    form->setSpacing(8);
    m_size = new QSpinBox;
    m_size->setRange(qrs::limits::MinPixelSize, qrs::limits::MaxPixelSize);
    m_size->setSingleStep(32);
    m_size->setSuffix(tr(" px"));
    m_margin = new QSpinBox;
    m_margin->setRange(0, qrs::limits::MaxMargin);
    m_margin->setSuffix(tr(" modules"));
    m_margin->setToolTip(tr("Empty border around the code. 4 is recommended."));
    m_ecc = new QComboBox;
    m_ecc->addItems({tr("Low (7%)"), tr("Medium (15%)"), tr("Quartile (25%)"), tr("High (30%)")});
    m_ecc->setToolTip(tr("How much damage the code can survive. Higher levels make a denser code."));
    m_fgButton = makeButton(QString());
    m_bgButton = makeButton(QString());
    auto *swap = makeButton(tr("Swap"));
    auto *reset = makeButton(tr("Reset"));
    // Two short rows instead of one long one, so the column never needs to scroll sideways.
    auto *colors = new QHBoxLayout;
    colors->addWidget(m_fgButton, 1);
    colors->addWidget(m_bgButton, 1);
    auto *colorTools = new QHBoxLayout;
    colorTools->addWidget(swap);
    colorTools->addWidget(reset);
    colorTools->addStretch();

    form->addRow(tr("Size"), m_size);
    form->addRow(tr("Margin"), m_margin);
    form->addRow(tr("Error correction"), m_ecc);
    form->addRow(tr("Colours"), colors);
    form->addRow(QString(), colorTools);
    card->addLayout(form);

    updateColorButton(m_fgButton, m_fg);
    updateColorButton(m_bgButton, m_bg);
    connect(m_size, &QSpinBox::valueChanged, this, [this] { scheduleUpdate(); });
    connect(m_margin, &QSpinBox::valueChanged, this, [this] { scheduleUpdate(); });
    connect(m_ecc, &QComboBox::currentIndexChanged, this, [this] { scheduleUpdate(); });
    connect(m_fgButton, &QPushButton::clicked, this, [this] { pickColor(&m_fg, m_fgButton); });
    connect(m_bgButton, &QPushButton::clicked, this, [this] { pickColor(&m_bg, m_bgButton); });
    connect(swap, &QPushButton::clicked, this, [this] {
        std::swap(m_fg, m_bg);
        updateColorButton(m_fgButton, m_fg);
        updateColorButton(m_bgButton, m_bg);
        scheduleUpdate();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        m_fg = Qt::black;
        m_bg = Qt::white;
        updateColorButton(m_fgButton, m_fg);
        updateColorButton(m_bgButton, m_bg);
        scheduleUpdate();
    });
    return frame;
}

} // namespace app
