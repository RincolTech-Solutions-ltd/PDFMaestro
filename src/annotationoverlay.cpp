#include "annotationoverlay.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QInputDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QCursor>
#include <cmath>

static const QStringList STAMP_NAMES = {
    "Approved","Draft","Confidential","Final","Void","ForComment","NotApproved"
};

static QCursor cursorForTool(const QString& t) {
    if (t == "highlight")  return QCursor(Qt::IBeamCursor);
    if (t == "note")       return QCursor(Qt::PointingHandCursor);
    if (t == "ink")        return QCursor(Qt::CrossCursor);
    if (t == "stamp")      return QCursor(Qt::PointingHandCursor);
    if (t == "redact")     return QCursor(Qt::CrossCursor);
    if (t == "signature")  return QCursor(Qt::SizeAllCursor);
    return QCursor(Qt::ArrowCursor);
}

AnnotationOverlay::AnnotationOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void AnnotationOverlay::setTool(const QString& tool) {
    m_tool = tool;
    setCursor(cursorForTool(tool));
    setAttribute(Qt::WA_TransparentForMouseEvents, tool == "pointer");
    resetDrawState();
    update();
}

void AnnotationOverlay::setPageContext(int pageIdx, const QRectF& pageRect,
                                        double pageHeightPt, double zoom) {
    m_pageIdx      = pageIdx;
    m_pageRect     = pageRect;
    m_pageHeightPt = pageHeightPt;
    m_zoom         = zoom;
}

double AnnotationOverlay::pageWidthPt() const {
    if (m_pageRect.height() == 0) return 595.0;
    return m_pageHeightPt * m_pageRect.width() / m_pageRect.height();
}

QPair<double,double> AnnotationOverlay::toPdf(const QPointF& pt) const {
    if (m_pageRect.width() == 0 || m_pageRect.height() == 0) return {0,0};
    double relX = (pt.x() - m_pageRect.x()) / m_pageRect.width();
    double relY = (pt.y() - m_pageRect.y()) / m_pageRect.height();
    return { relX * pageWidthPt(), (1.0 - relY) * m_pageHeightPt };
}

void AnnotationOverlay::setSignatureImage(const QImage& img, double sigW) {
    m_sigImage = img;
    m_sigW = sigW;
    if (!img.isNull() && img.width() > 0)
        m_sigH = sigW * static_cast<double>(img.height()) / img.width();
    else
        m_sigH = sigW * 0.4; // fallback aspect
}

void AnnotationOverlay::resetDrawState() {
    m_dragging = false;
    m_inkStrokes.clear();
    m_inkCurrent.clear();
}

void AnnotationOverlay::mousePressEvent(QMouseEvent* event) {
    try {
        if (m_tool == "pointer" || !m_pageRect.contains(event->position())) {
            event->ignore();
            return;
        }
        QPointF pos = event->position();
        if (m_tool == "highlight" || m_tool == "redact") {
            m_dragStart = pos;
            m_dragCur   = pos;
            m_dragging  = true;
        } else if (m_tool == "ink") {
            auto [px, py] = toPdf(pos);
            m_inkCurrent = { QPointF(px, py) };
        } else if (m_tool == "note") {
            commitNote(pos);
        } else if (m_tool == "stamp") {
            commitStamp(pos);
        } else if (m_tool == "signature") {
            commitSignature(pos);
        }
        update();
    } catch (const std::exception& e) { qWarning() << e.what(); }
}

void AnnotationOverlay::mouseMoveEvent(QMouseEvent* event) {
    try {
        QPointF pos = event->position();
        if ((m_tool == "highlight" || m_tool == "redact") && m_dragging) {
            m_dragCur = pos;
            update();
        } else if (m_tool == "ink" && !m_inkCurrent.isEmpty()) {
            auto [px, py] = toPdf(pos);
            m_inkCurrent.append(QPointF(px, py));
            update();
        } else if (m_tool == "signature") {
            m_sigGhostPos = pos;
            update();
        }
    } catch (const std::exception& e) { qWarning() << e.what(); }
}

void AnnotationOverlay::mouseReleaseEvent(QMouseEvent* event) {
    try {
        QPointF pos = event->position();
        if (m_tool == "highlight" && m_dragging) {
            commitHighlight(m_dragStart, pos);
            m_dragging = false;
            update();
        } else if (m_tool == "redact" && m_dragging) {
            commitRedact(m_dragStart, pos);
            m_dragging = false;
            update();
        } else if (m_tool == "ink") {
            if (m_inkCurrent.size() >= 2)
                m_inkStrokes.append(m_inkCurrent);
            m_inkCurrent.clear();
            update();
        }
    } catch (const std::exception& e) { qWarning() << e.what(); }
}

void AnnotationOverlay::keyPressEvent(QKeyEvent* event) {
    if (m_tool == "signature" && event->key() == Qt::Key_Escape) {
        m_sigImage = QImage();
        setTool("pointer");
        emit signatureCancelled();
    }
    QWidget::keyPressEvent(event);
}

void AnnotationOverlay::mouseDoubleClickEvent(QMouseEvent*) {
    try {
        if (m_tool == "ink" && !m_inkStrokes.isEmpty()) {
            commitInk();
            resetDrawState();
            update();
        }
    } catch (const std::exception& e) { qWarning() << e.what(); }
}

void AnnotationOverlay::commitHighlight(const QPointF& p1, const QPointF& p2) {
    auto [x0,y0] = toPdf(QPointF(std::min(p1.x(),p2.x()), std::min(p1.y(),p2.y())));
    auto [x1,y1] = toPdf(QPointF(std::max(p1.x(),p2.x()), std::max(p1.y(),p2.y())));
    if (std::abs(x1-x0) < 2 || std::abs(y1-y0) < 2) return;
    emit annotationCommitted({
        {"type",    "highlight"},
        {"page",    m_pageIdx},
        {"x0",      x0}, {"y0", y0}, {"x1", x1}, {"y1", y1}
    });
}

void AnnotationOverlay::commitNote(const QPointF& pos) {
    bool ok;
    QString text = QInputDialog::getMultiLineText(
        this, "Sticky Note", "Note text:", {}, &ok);
    if (!ok || text.trimmed().isEmpty()) return;
    auto [x, y] = toPdf(pos);
    emit annotationCommitted({
        {"type",     "note"},
        {"page",     m_pageIdx},
        {"x",        x}, {"y", y},
        {"contents", text.trimmed()}
    });
}

void AnnotationOverlay::commitInk() {
    if (m_inkStrokes.isEmpty()) return;
    QVariantList strokes;
    for (const auto& s : m_inkStrokes) {
        QVariantList pts;
        for (const auto& p : s) pts << QVariant::fromValue(p);
        strokes << QVariant(pts);
    }
    emit annotationCommitted({
        {"type",    "ink"},
        {"page",    m_pageIdx},
        {"strokes", strokes}
    });
}

void AnnotationOverlay::commitStamp(const QPointF& pos) {
    QDialog dlg(this);
    dlg.setWindowTitle("Insert Stamp");
    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel("Stamp type:"));
    auto* combo = new QComboBox;
    combo->addItems(STAMP_NAMES);
    layout->addWidget(combo);
    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted) return;

    auto [x, y] = toPdf(pos);
    emit annotationCommitted({
        {"type",  "stamp"},
        {"page",  m_pageIdx},
        {"x",     x}, {"y", y},
        {"w",     144.0}, {"h", 48.0},
        {"name",  combo->currentText()}
    });
}

void AnnotationOverlay::commitSignature(const QPointF& screenPos) {
    if (m_sigImage.isNull()) return;
    if (!m_pageRect.contains(screenPos)) return;

    // Ghost is centered on cursor. The bottom-left in PDF space is what
    // overlayImageOnPage expects (x = left edge, y = bottom edge).
    double pxPerPt = (m_pageRect.width() > 0 && pageWidthPt() > 0)
                     ? m_pageRect.width() / pageWidthPt() : 1.0;
    double ghostW  = m_sigW * pxPerPt;
    double ghostH  = m_sigH * pxPerPt;

    // Bottom-left screen position of the centered ghost
    QPointF screenBL(screenPos.x() - ghostW / 2.0,
                     screenPos.y() + ghostH / 2.0);

    auto [pdfX, pdfY] = toPdf(screenBL);
    emit annotationCommitted({
        {"type",  "signature"},
        {"page",  m_pageIdx},
        {"x",     pdfX},
        {"y",     pdfY},
        {"sigW",  m_sigW},
        {"sigH",  m_sigH}
    });
}

void AnnotationOverlay::commitRedact(const QPointF& p1, const QPointF& p2) {
    auto [x0,y0] = toPdf(QPointF(std::min(p1.x(),p2.x()), std::min(p1.y(),p2.y())));
    auto [x1,y1] = toPdf(QPointF(std::max(p1.x(),p2.x()), std::max(p1.y(),p2.y())));
    if (std::abs(x1-x0) < 2 || std::abs(y1-y0) < 2) return;
    emit annotationCommitted({
        {"type", "redact"},
        {"page", m_pageIdx},
        {"x0",   x0}, {"y0", y0}, {"x1", x1}, {"y1", y1}
    });
}

// ── Paint ─────────────────────────────────────────────────────────────────────

static QVector<QPointF> pdfStrokeToScreen(const QVector<QPointF>& stroke,
                                           const QRectF& pr, double pw, double ph) {
    QVector<QPointF> out;
    for (const auto& p : stroke) {
        double sx = pr.x() + (p.x() / pw) * pr.width();
        double sy = pr.y() + (1.0 - p.y() / ph) * pr.height();
        out.append({sx, sy});
    }
    return out;
}

void AnnotationOverlay::paintEvent(QPaintEvent*) {
    if (m_tool == "pointer") return;

    // ── Signature ghost ───────────────────────────────────────────────────────
    if (m_tool == "signature" && !m_sigImage.isNull()
        && m_pageRect.contains(m_sigGhostPos))
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        double pxPerPt = (m_pageRect.width() > 0 && pageWidthPt() > 0)
                         ? m_pageRect.width() / pageWidthPt() : 1.0;
        double ghostW = m_sigW * pxPerPt;
        double ghostH = m_sigH * pxPerPt;

        // Draw centered on cursor, 70% opacity
        QRectF ghostRect(m_sigGhostPos.x() - ghostW / 2.0,
                         m_sigGhostPos.y() - ghostH / 2.0,
                         ghostW, ghostH);
        p.setOpacity(0.70);
        p.drawImage(ghostRect, m_sigImage);
        p.setOpacity(1.0);

        // Dashed border
        p.setPen(QPen(QColor(0, 120, 215), 1.5, Qt::DashLine));
        p.drawRect(ghostRect);

        // Hint text
        p.setPen(QColor(60, 60, 60));
        p.setFont(QFont("sans-serif", 9));
        p.drawText(m_pageRect.bottomLeft() + QPointF(4, -6),
                   "Click to place signature  •  Escape to cancel");
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if ((m_tool == "highlight" || m_tool == "redact") && m_dragging) {
        QColor col = (m_tool == "highlight")
                     ? QColor(255,230,0,100) : QColor(0,0,0,160);
        p.fillRect(QRectF(m_dragStart, m_dragCur).normalized(), col);
        p.setPen(QPen(col.darker(140), 1, Qt::DashLine));
        p.drawRect(QRectF(m_dragStart, m_dragCur).normalized());
    }

    if (m_tool == "ink") {
        p.setPen(QPen(QColor(0,0,200,200), 2.5, Qt::SolidLine,
                      Qt::RoundCap, Qt::RoundJoin));
        double pw = pageWidthPt();
        for (const auto& stroke : m_inkStrokes) {
            auto pts = pdfStrokeToScreen(stroke, m_pageRect, pw, m_pageHeightPt);
            for (int i = 1; i < pts.size(); ++i)
                p.drawLine(pts[i-1], pts[i]);
        }
        if (!m_inkCurrent.isEmpty()) {
            auto pts = pdfStrokeToScreen(m_inkCurrent, m_pageRect, pw, m_pageHeightPt);
            for (int i = 1; i < pts.size(); ++i)
                p.drawLine(pts[i-1], pts[i]);
        }
        if (!m_inkStrokes.isEmpty()) {
            p.setFont(QFont("sans-serif", 9));
            p.setPen(QColor(80,80,80));
            p.drawText(m_pageRect.bottomLeft() + QPointF(4,-6),
                       "Double-click to commit ink");
        }
    }
}
