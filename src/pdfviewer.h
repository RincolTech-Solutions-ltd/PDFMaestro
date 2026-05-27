#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QByteArray>
#include <memory>
#include <poppler-qt6.h>

class AnnotationOverlay;

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

signals:
    void pageChanged(int current, int total);   // 1-based
    void zoomChanged(double zoom);
    void documentLoaded(const QString& path, int count);
    void annotationCommitted(const QVariantMap& payload);
    void signatureCancelled();

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
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void buildScene();
    void rerenderAtZoom();
    void pushPageContext();
    void updateCurrentPageFromScroll();
    QPixmap renderPage(int idx);

    QGraphicsScene*  m_scene;
    QTimer*          m_rerenderTimer;

    std::unique_ptr<Poppler::Document> m_doc;
    QString          m_displayPath;
    QList<QGraphicsPixmapItem*> m_pageItems;

    int    m_currentPage = 0;
    double m_zoom        = 1.0;

    AnnotationOverlay* m_overlay = nullptr;

    static constexpr double BASE_DPI  = 150.0;
    static constexpr int    PAGE_GAP  = 16;
    static constexpr double ZOOM_MIN  = 0.10;
    static constexpr double ZOOM_MAX  = 5.00;
};
