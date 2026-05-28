#pragma once
#include <QWidget>
#include <QRectF>
#include <QPointF>
#include <QVariantMap>
#include <QImage>

class AnnotationOverlay : public QWidget {
    Q_OBJECT
public:
    explicit AnnotationOverlay(QWidget* parent = nullptr);

    void setTool(const QString& tool);
    void setPageContext(int pageIdx, const QRectF& pageRect,
                       double pageHeightPt, double zoom);

    // Signature drag-to-place: call this, then set tool to "signature"
    void setSignatureImage(const QImage& img, double sigWPt = 150.0);

signals:
    void annotationCommitted(const QVariantMap& payload);
    void signaturePlacementDone();   // emitted after commit OR Escape

protected:
    void mousePressEvent(QMouseEvent* event)      override;
    void mouseMoveEvent(QMouseEvent* event)       override;
    void mouseReleaseEvent(QMouseEvent* event)    override;
    void mouseDoubleClickEvent(QMouseEvent* event)override;
    void keyPressEvent(QKeyEvent* event)          override;
    void paintEvent(QPaintEvent* event)           override;

private:
    QPair<double,double> toPdf(const QPointF& pt) const;
    double pageWidthPt() const;
    void resetDrawState();

    void commitHighlight(const QPointF& p1, const QPointF& p2);
    void commitNote(const QPointF& pos);
    void commitInk();
    void commitStamp(const QPointF& pos);
    void commitRedact(const QPointF& p1, const QPointF& p2);
    void commitSignature(const QPointF& screenPos);
    void commitAddText(const QPointF& screenPos);

    QString m_tool    = "pointer";
    int     m_pageIdx = 0;
    QRectF  m_pageRect;
    double  m_pageHeightPt = 792.0;
    double  m_zoom         = 1.0;

    QPointF m_dragStart;
    QPointF m_dragCur;
    bool    m_dragging = false;

    QVector<QVector<QPointF>> m_inkStrokes;  // PDF coords
    QVector<QPointF>          m_inkCurrent;  // PDF coords

    // Signature placement state
    QImage  m_sigImage;
    double  m_sigWPt    = 150.0;
    double  m_sigHPt    = 60.0;
    QPointF m_sigGhostPos;
};
