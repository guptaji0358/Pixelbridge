#include "Widgets.h"

#include "Theme.h"

#include <QDragEnterEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QVBoxLayout>

namespace app {

QLabel *makeLabel(const QString &text, const char *role, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    if (role)
        label->setProperty("role", role);
    return label;
}

QPushButton *makeButton(const QString &text, const char *kind, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    if (kind)
        button->setProperty("kind", kind);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QFrame *makeCard(QVBoxLayout **layout, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("card");
    auto *box = new QVBoxLayout(card);
    box->setContentsMargins(16, 16, 16, 16);
    box->setSpacing(10);
    if (layout)
        *layout = box;
    return card;
}

QWidget *makeHeader(const QString &title, const QString &subtitle, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *box = new QVBoxLayout(w);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(2);
    box->addWidget(makeLabel(title, "title"));
    auto *sub = makeLabel(subtitle, "subtitle");
    sub->setWordWrap(true);
    box->addWidget(sub);
    return w;
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

// ------------------------------------------------------------ ImageView ----

ImageView::ImageView(QWidget *parent) : QFrame(parent)
{
    setMinimumSize(220, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImageView::setPlaceholder(const QString &title, const QString &hint)
{
    m_title = title;
    m_hint = hint;
    update();
}

void ImageView::setImage(const QImage &image)
{
    m_image = image;
    m_outlines.clear();
    update();
}

void ImageView::setOutlines(const QVector<Outline> &outlines)
{
    m_outlines = outlines;
    update();
}

void ImageView::setCheckerboard(bool on)
{
    m_backdrop = on;
    update();
}

void ImageView::clear()
{
    m_image = QImage();
    m_outlines.clear();
    update();
}

void ImageView::setAcceptFileDrops(bool on) { setAcceptDrops(on); }

QRectF ImageView::imageRect() const
{
    if (m_image.isNull())
        return {};
    const QRectF area = QRectF(rect()).adjusted(12, 12, -12, -12);
    QSizeF size = m_image.size();
    size.scale(area.size(), Qt::KeepAspectRatio);
    // Never enlarge small images beyond 4x: keeps pixel art crisp but not absurd.
    const qreal maxScale = 4.0;
    if (size.width() > m_image.width() * maxScale)
        size = QSizeF(m_image.size()) * maxScale;
    return QRectF(area.center() - QPointF(size.width() / 2, size.height() / 2), size);
}

void ImageView::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = currentTheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(frame, 10, 10);
    p.fillPath(path, m_backdrop && !m_image.isNull() ? c.surfaceAlt : c.surface);
    QPen border(m_dragActive ? c.accent : c.border, m_dragActive ? 2 : 1);
    if (m_image.isNull())
        border.setStyle(Qt::DashLine);
    p.setPen(border);
    p.drawPath(path);

    if (m_image.isNull()) {
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() + 2);
        f.setWeight(QFont::DemiBold);
        const QRect area = rect().adjusted(24, 0, -24, 0);
        p.setFont(f);
        p.setPen(c.text);
        p.drawText(area.adjusted(0, 0, 0, -26), Qt::AlignCenter | Qt::TextWordWrap, m_title);
        p.setFont(font());
        p.setPen(c.textMuted);
        p.drawText(area.adjusted(0, 30, 0, 0), Qt::AlignCenter | Qt::TextWordWrap, m_hint);
        return;
    }

    const QRectF target = imageRect();
    const bool upscaling = target.width() > m_image.width();
    p.setRenderHint(QPainter::SmoothPixmapTransform, !upscaling);
    p.drawImage(target, m_image);

    if (!m_outlines.isEmpty()) {
        const qreal sx = target.width() / m_image.width();
        const qreal sy = target.height() / m_image.height();
        for (const Outline &o : m_outlines) {
            QPolygonF poly;
            for (const QPoint &pt : o.polygon)
                poly << QPointF(target.left() + pt.x() * sx, target.top() + pt.y() * sy);
            QColor color = o.ok ? c.success : c.warning;
            // Decoded codes get an outline only, so the picture is not tinted;
            // unreadable candidates are shaded to draw the eye to the problem area.
            QColor fill = color;
            fill.setAlpha(o.ok ? 0 : 45);
            p.setBrush(fill);
            p.setPen(QPen(color, 2.5, o.ok ? Qt::SolidLine : Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolygon(poly);
        }
    }
}

static QString droppedFile(const QMimeData *mime)
{
    if (!mime->hasUrls())
        return {};
    for (const QUrl &url : mime->urls())
        if (url.isLocalFile())
            return url.toLocalFile();
    return {};
}

void ImageView::dragEnterEvent(QDragEnterEvent *e)
{
    if (!droppedFile(e->mimeData()).isEmpty()) {
        e->acceptProposedAction();
        m_dragActive = true;
        update();
    }
}

void ImageView::dragLeaveEvent(QDragLeaveEvent *)
{
    m_dragActive = false;
    update();
}

void ImageView::dropEvent(QDropEvent *e)
{
    m_dragActive = false;
    update();
    const QString path = droppedFile(e->mimeData());
    if (!path.isEmpty()) {
        e->acceptProposedAction();
        emit fileDropped(path);
    }
}

void ImageView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->pos()))
        emit clicked();
}

} // namespace app
