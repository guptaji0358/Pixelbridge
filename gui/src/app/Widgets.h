// Small shared UI building blocks.
#pragma once

#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QPolygon>
#include <QPushButton>
#include <QVector>

class QVBoxLayout;

namespace app {

QLabel *makeLabel(const QString &text, const char *role = nullptr, QWidget *parent = nullptr);
QPushButton *makeButton(const QString &text, const char *kind = nullptr, QWidget *parent = nullptr);

// Rounded surface panel; returns the frame, `layout` receives its VBox.
QFrame *makeCard(QVBoxLayout **layout, QWidget *parent = nullptr);

// Page header: large title with a muted one-line description underneath.
QWidget *makeHeader(const QString &title, const QString &subtitle, QWidget *parent = nullptr);

// Re-evaluates style-sheet rules after a dynamic property changed.
void repolish(QWidget *widget);

// Aspect-preserving image display with an empty state, optional outlines
// (in image coordinates) and file drag-and-drop.
class ImageView : public QFrame {
    Q_OBJECT
public:
    struct Outline {
        QPolygon polygon;
        bool ok = true; // false = located but undecodable (drawn in the warning colour)
    };

    explicit ImageView(QWidget *parent = nullptr);

    void setPlaceholder(const QString &title, const QString &hint);
    void setImage(const QImage &image);
    void setOutlines(const QVector<Outline> &outlines);
    void setCheckerboard(bool on); // show a neutral backdrop behind the image
    void clear();
    const QImage &image() const { return m_image; }
    void setAcceptFileDrops(bool on);

signals:
    void fileDropped(const QString &path);
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QRectF imageRect() const;

    QImage m_image;
    QVector<Outline> m_outlines;
    QString m_title, m_hint;
    bool m_dragActive = false;
    bool m_backdrop = false;
};

} // namespace app
