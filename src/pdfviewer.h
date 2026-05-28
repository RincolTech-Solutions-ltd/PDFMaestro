#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QByteArray>
#include <QImage>
#include <QList>
#include <memory>
#include <poppler-qt6.h>

class AnnotationOverlay;

// ── Pending signature overlay record ─────────────────────────────────────────
// Kept in the QGraphicsScene as a draggable item; burned into QPDF on Save only.
struct SigRecord {
    QImage  image;
    int     pageIdx  = 0;
    double  xPdf     = 0;   // bottom-left x in PDF points (updated before save)
    double  yPdf     = 0;   // bottom-left y in PDF points (PDF Y-axis = up from bottom)
    double  sigWPt   = 150;
    double  sigHPt   = 60;
    QGraphicsItem* item = nullptr;  // owned by QGraphicsScene
};

// Returned by takePendingSignatures() for MainWindow to burn into QPDF
struct SigCoords {
    QImage  image;
    int     page;
    double  x, y, sigW, sigH;  // all in PDF points
};

// ── PdfViewer ─────────────────────────────────────────────────────────────────
class PdfViewer : public QGraphicsView {
    Q_OBJECT
public:
    explicit PdfViewer(QWidget* parent = nullptr);
    ~PdfViewer();

    void loadFromBytes(const QByteArray& data, const QString& displayPath = {});
    void loadFile(const QString& path);

    int  currentPage()  const { return m_currentPage; }
    int  pageCount()    const;
    double zoom()       const { return m_zoom; }

    Poppler::Document* document() const { return m_doc.get(); }
    void clear();

    void setAnnotationTool(const QString& tool);
    void beginSignaturePlacement(const QImage& img);

    // ── Pending signature overlay API ────────────────────────────────────────
    // Convert a viewport-pixel point to (pageIdx, PDF x, PDF y).
    // Uses mapToScene() — always exact regardless of scroll position.
    struct PagePdfPos { int pageIdx; double xPdf; double yPdf; };
    PagePdfPos viewportToPdf(QPointF vpPt) const;

    // Add a moveable signature overlay at the given PDF coordinates.
    void addSigOverlay(const QImage& img, int pageIdx,
                       double xPdf, double yPdf, double sigWPt, double sigHPt);

    // Add a moveable signature overlay using raw viewport-pixel click centre.
    // Converts via mapToScene() — always exact regardless of scroll position.
    void addSigOverlayAtViewport(const QImage& img, QPointF vpCenter,
                                 double sigWPt, double sigHPt);

    // Harvest current drag positions → PDF coords, remove all items, return records.
    // Call this immediately before writing QPDF to disk.
    QList<SigCoords> takePendingSignatures();

    // Discard all pending sigs without burning (on close / new document).
    void clearPendingSignatures();

signals:
    void pageChanged(int current, int total);   // 1-based
    void zoomChanged(double zoom);
    void documentLoaded(const QString& path, int count);
    void annotationCommitted(const QVariantMap& payload);
    void signatureCancelled();   // emitted when placement done (placed or Escaped)

public slots:
    void goToPage(int index);   // 0-based
    void nextPage();
    void prevPage();
    void firstPage();
    void lastPage();
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void fitWidth();
    void fitPage();
    void setZoom(double zoom);

protected:
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event)        override;
    void keyPressEvent(QKeyEvent* event)       override;
    void resizeEvent(QResizeEvent* event)      override;
    void scrollContentsBy(int dx, int dy)      override;

private:
    void buildScene();
    void rerenderAtZoom();
    void pushPageContext();
    void updateCurrentPageFromScroll();
    QPixmap renderPage(int idx);

    // Pending sig helpers
    void   refreshSigItems();          // re-render all sig items after zoom/rebuild
    void   harvestSigPosition(SigRecord& rec);  // read item->pos() → update xPdf/yPdf
    double getPageHeightPt(int idx) const;

    QGraphicsScene*  m_scene;
    QTimer*          m_rerenderTimer;

    std::unique_ptr<Poppler::Document> m_doc;
    QString          m_displayPath;
    QList<QGraphicsPixmapItem*> m_pageItems;

    int    m_currentPage = 0;
    double m_zoom        = 1.0;

    AnnotationOverlay* m_overlay = nullptr;

    QList<SigRecord>   m_sigRecords;   // pending sigs not yet burned into QPDF
    bool               m_sigItemDragging = false;  // true while dragging a sig item

    static constexpr double BASE_DPI  = 150.0;
    static constexpr int    PAGE_GAP  = 16;
    static constexpr double ZOOM_MIN  = 0.10;
    static constexpr double ZOOM_MAX  = 5.00;
};
