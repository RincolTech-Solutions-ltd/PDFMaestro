#include "pdfviewer.h"
#include "annotationoverlay.h"
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QDebug>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

// ── Moveable signature scene item ─────────────────────────────────────────────
// Lives in the QGraphicsScene; draggable until the user saves the document.
class SigSceneItem : public QGraphicsPixmapItem {
public:
    enum { Type = QGraphicsItem::UserType + 42 };
    int type() const override { return Type; }

    explicit SigSceneItem(const QPixmap& pm) : QGraphicsPixmapItem(pm) {
        setFlag(ItemIsMovable);
        setFlag(ItemIsSelectable);
        setFlag(ItemSendsGeometryChanges);
        setZValue(200);   // always above page content
        setTransformationMode(Qt::SmoothTransformation);
    }

protected:
    void paint(QPainter* p,
               const QStyleOptionGraphicsItem* /*opt*/,
               QWidget* w) override
    {
        // Draw image normally (skip Qt's built-in selection decoration)
        QStyleOptionGraphicsItem plain;
        plain.state = QStyle::State_None;
        QGraphicsPixmapItem::paint(p, &plain, w);

        // Dashed border — blue when selected, faint grey otherwise
        QRectF r = boundingRect().adjusted(1, 1, -1, -1);
        if (isSelected()) {
            p->setPen(QPen(QColor(60, 120, 255), 2.0, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRect(r);
            p->setFont(QFont("sans-serif", 8));
            p->setPen(QColor(40, 100, 220));
            p->drawText(r.bottomLeft() + QPointF(2, 14),
                        "Drag to reposition  •  Delete to remove");
        } else {
            p->setPen(QPen(QColor(120, 160, 255, 140), 1.0, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRect(r);
        }
    }
};

PdfViewer::PdfViewer(QWidget* parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_rerenderTimer(new QTimer(this))
{
    setScene(m_scene);
    setDragMode(ScrollHandDrag);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setBackgroundBrush(QColor("#1e2530"));
    setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    setFocusPolicy(Qt::StrongFocus);

    m_rerenderTimer->setSingleShot(true);
    m_rerenderTimer->setInterval(350);
    connect(m_rerenderTimer, &QTimer::timeout, this, &PdfViewer::rerenderAtZoom);

    m_overlay = new AnnotationOverlay(viewport());
    m_overlay->setGeometry(viewport()->rect());
    m_overlay->raise();
    connect(m_overlay, &AnnotationOverlay::annotationCommitted,
            this,      &PdfViewer::annotationCommitted);
    // Restore pointer mode and notify MainWindow when signature placement ends
    connect(m_overlay, &AnnotationOverlay::signaturePlacementDone, this, [this](){
        setAnnotationTool("pointer");
        emit signatureCancelled();
    });
}

PdfViewer::~PdfViewer() = default;

void PdfViewer::clear() {
    clearPendingSignatures();
    m_doc.reset();
    m_scene->clear();
    m_pageItems.clear();
    m_currentPage = 0;
    m_zoom = 1.0;
}

int PdfViewer::pageCount() const {
    return m_doc ? m_doc->numPages() : 0;
}

void PdfViewer::loadFromBytes(const QByteArray& data, const QString& displayPath) {
    m_doc = Poppler::Document::loadFromData(data);
    if (!m_doc || m_doc->isLocked()) {
        qWarning() << "PdfViewer: failed to load document from bytes";
        return;
    }
    m_doc->setRenderHint(Poppler::Document::Antialiasing);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing);
    m_displayPath  = displayPath;
    m_currentPage  = 0;
    m_zoom         = 1.0;
    buildScene();
    emit documentLoaded(m_displayPath, m_doc->numPages());
    emit pageChanged(1, m_doc->numPages());
    emit zoomChanged(m_zoom);
}

void PdfViewer::loadFile(const QString& path) {
    m_doc = Poppler::Document::load(path);
    if (!m_doc || m_doc->isLocked()) {
        qWarning() << "PdfViewer: failed to load" << path;
        return;
    }
    m_doc->setRenderHint(Poppler::Document::Antialiasing);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing);
    m_displayPath  = path;
    m_currentPage  = 0;
    m_zoom         = 1.0;
    buildScene();
    emit documentLoaded(path, m_doc->numPages());
    emit pageChanged(1, m_doc->numPages());
    emit zoomChanged(m_zoom);
}

QPixmap PdfViewer::renderPage(int idx) {
    if (!m_doc || idx < 0 || idx >= m_doc->numPages()) return {};
    std::unique_ptr<Poppler::Page> p(m_doc->page(idx));
    if (!p) return {};
    double dpi = BASE_DPI * m_zoom;
    QImage img = p->renderToImage(dpi, dpi);
    return QPixmap::fromImage(img);
}

void PdfViewer::buildScene() {
    // Harvest current drag positions before wiping the scene
    for (auto& rec : m_sigRecords) harvestSigPosition(rec);

    m_scene->clear();   // removes ALL items including old sig items
    m_pageItems.clear();
    // Nullify sig item pointers — scene deleted them
    for (auto& rec : m_sigRecords) rec.item = nullptr;

    if (!m_doc) return;

    int y = PAGE_GAP;
    for (int i = 0; i < m_doc->numPages(); ++i) {
        QPixmap pm = renderPage(i);
        auto item = m_scene->addPixmap(pm);
        item->setTransformationMode(Qt::SmoothTransformation);
        item->setPos(-pm.width() / 2.0, y);
        m_pageItems.append(item);
        y += pm.height() + PAGE_GAP;
    }
    auto r = m_scene->itemsBoundingRect();
    setSceneRect(r.adjusted(-20, -PAGE_GAP, 20, PAGE_GAP));
    pushPageContext();
    refreshSigItems();   // re-add pending sigs at their stored positions
}

void PdfViewer::rerenderAtZoom() {
    if (!m_doc) return;
    try {
        // Harvest current drag positions at old zoom before page items move
        for (auto& rec : m_sigRecords) harvestSigPosition(rec);

        int y = PAGE_GAP;
        for (int i = 0; i < (int)m_pageItems.size(); ++i) {
            QPixmap pm = renderPage(i);
            m_pageItems[i]->setPixmap(pm);
            m_pageItems[i]->setPos(-pm.width() / 2.0, y);
            y += pm.height() + PAGE_GAP;
        }
        auto r = m_scene->itemsBoundingRect();
        setSceneRect(r.adjusted(-20, -PAGE_GAP, 20, PAGE_GAP));
        pushPageContext();
        refreshSigItems();   // re-render at new zoom level
    } catch (const std::exception& e) {
        qWarning() << "PdfViewer::rerenderAtZoom:" << e.what();
    }
}

void PdfViewer::pushPageContext() {
    if (!m_overlay || m_pageItems.isEmpty() ||
        m_currentPage >= m_pageItems.size()) return;

    auto item   = m_pageItems[m_currentPage];
    auto scBr   = item->sceneBoundingRect();
    QPointF tl  = mapFromScene(scBr.topLeft());
    QPointF br  = mapFromScene(scBr.bottomRight());

    double hPt = 792.0;
    if (m_doc && m_currentPage < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(m_currentPage));
        if (p) hPt = p->pageSizeF().height();
    }
    m_overlay->setPageContext(m_currentPage, QRectF(tl, br), hPt, m_zoom);
}

void PdfViewer::setAnnotationTool(const QString& tool) {
    m_overlay->setTool(tool);
    setDragMode(tool == "pointer" ? ScrollHandDrag : NoDrag);
}

void PdfViewer::beginSignaturePlacement(const QImage& img) {
    m_overlay->setSignatureImage(img);
    setAnnotationTool("signature");
}

void PdfViewer::goToPage(int index) {
    if (m_pageItems.isEmpty()) return;
    index = qBound(0, index, m_pageItems.size() - 1);
    m_currentPage = index;
    centerOn(m_pageItems[index]);
    emit pageChanged(index + 1, m_doc ? m_doc->numPages() : 1);
    pushPageContext();
}

void PdfViewer::nextPage()  { goToPage(m_currentPage + 1); }
void PdfViewer::prevPage()  { goToPage(m_currentPage - 1); }
void PdfViewer::firstPage() { goToPage(0); }
void PdfViewer::lastPage()  { goToPage(m_pageItems.size() - 1); }

void PdfViewer::setZoom(double zoom) {
    m_zoom = qBound(ZOOM_MIN, zoom, ZOOM_MAX);
    m_rerenderTimer->start();
    emit zoomChanged(m_zoom);
}

void PdfViewer::zoomIn()    { setZoom(m_zoom * 1.25); }
void PdfViewer::zoomOut()   { setZoom(m_zoom / 1.25); }
void PdfViewer::zoomReset() { setZoom(1.0); }

void PdfViewer::fitWidth() {
    if (!m_doc || m_doc->numPages() == 0) return;
    std::unique_ptr<Poppler::Page> p(m_doc->page(m_currentPage));
    if (!p) return;
    double baseW = p->pageSizeF().width() * BASE_DPI / 72.0;
    double vw    = viewport()->width() - 40;
    setZoom(vw / baseW);
}

void PdfViewer::fitPage() {
    if (!m_doc || m_doc->numPages() == 0) return;
    std::unique_ptr<Poppler::Page> p(m_doc->page(m_currentPage));
    if (!p) return;
    double baseW = p->pageSizeF().width()  * BASE_DPI / 72.0;
    double baseH = p->pageSizeF().height() * BASE_DPI / 72.0;
    double vw = viewport()->width()  - 40;
    double vh = viewport()->height() - 40;
    setZoom(std::min(vw / baseW, vh / baseH));
}

void PdfViewer::updateCurrentPageFromScroll() {
    if (m_pageItems.isEmpty()) return;
    QPointF cy = mapToScene(viewport()->rect().center());
    int closest = 0;
    double minD = 1e18;
    for (int i = 0; i < m_pageItems.size(); ++i) {
        auto it = m_pageItems[i];
        double pageCy = it->y() + it->pixmap().height() / 2.0;
        double d = std::abs(pageCy - cy.y());
        if (d < minD) { minD = d; closest = i; }
    }
    if (closest != m_currentPage) {
        m_currentPage = closest;
        emit pageChanged(closest + 1, m_doc ? m_doc->numPages() : 1);
        pushPageContext();
    }
}

void PdfViewer::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) zoomIn(); else zoomOut();
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
        updateCurrentPageFromScroll();
    }
}

void PdfViewer::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace: {
        // Remove any selected pending-signature items
        bool removed = false;
        for (int i = m_sigRecords.size() - 1; i >= 0; --i) {
            if (m_sigRecords[i].item && m_sigRecords[i].item->isSelected()) {
                m_scene->removeItem(m_sigRecords[i].item);
                delete m_sigRecords[i].item;
                m_sigRecords.remove(i);
                removed = true;
            }
        }
        if (!removed) QGraphicsView::keyPressEvent(event);
        break;
    }
    case Qt::Key_Right: case Qt::Key_PageDown: nextPage();  break;
    case Qt::Key_Left:  case Qt::Key_PageUp:   prevPage();  break;
    case Qt::Key_Home:                         firstPage(); break;
    case Qt::Key_End:                          lastPage();  break;
    case Qt::Key_Equal:
        if (event->modifiers() & Qt::ControlModifier) zoomIn();
        else QGraphicsView::keyPressEvent(event);
        break;
    case Qt::Key_Minus:
        if (event->modifiers() & Qt::ControlModifier) zoomOut();
        else QGraphicsView::keyPressEvent(event);
        break;
    case Qt::Key_0:
        if (event->modifiers() & Qt::ControlModifier) zoomReset();
        else QGraphicsView::keyPressEvent(event);
        break;
    default:
        QGraphicsView::keyPressEvent(event);
    }
}

void PdfViewer::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (m_overlay) m_overlay->setGeometry(viewport()->rect());
    pushPageContext();
}

void PdfViewer::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    updateCurrentPageFromScroll();
    pushPageContext();
}

// ── Pending signature overlay ─────────────────────────────────────────────────

double PdfViewer::getPageHeightPt(int idx) const {
    if (m_doc && idx >= 0 && idx < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(idx));
        if (p) return p->pageSizeF().height();
    }
    return 792.0;  // US Letter fallback
}

// Read item's current scene position and back-compute PDF bottom-left coords.
void PdfViewer::harvestSigPosition(SigRecord& rec) {
    if (!rec.item || rec.pageIdx < 0 || rec.pageIdx >= m_pageItems.size()) return;
    auto* pageItem = m_pageItems[rec.pageIdx];
    double pxPerPt = BASE_DPI * m_zoom / 72.0;
    double ix = pageItem->pos().x();
    double iy = pageItem->pos().y();
    QPointF tl = rec.item->pos();   // scene top-left of sig item
    double pageHPt = getPageHeightPt(rec.pageIdx);
    rec.xPdf = (tl.x() - ix) / pxPerPt;
    rec.yPdf = pageHPt - (tl.y() - iy) / pxPerPt - rec.sigHPt;
}

// Create/recreate sig items in the scene at their current stored PDF positions.
void PdfViewer::refreshSigItems() {
    for (auto& rec : m_sigRecords) {
        // Remove stale item if still in scene
        if (rec.item) {
            m_scene->removeItem(rec.item);
            delete rec.item;
            rec.item = nullptr;
        }
        if (rec.pageIdx < 0 || rec.pageIdx >= m_pageItems.size()) continue;

        double pxPerPt   = BASE_DPI * m_zoom / 72.0;
        double pageHPt   = getPageHeightPt(rec.pageIdx);
        auto*  pageItem  = m_pageItems[rec.pageIdx];
        double ix        = pageItem->pos().x();
        double iy        = pageItem->pos().y();

        // Scale pixmap to current zoom
        int sw = qMax(1, qRound(rec.sigWPt * pxPerPt));
        int sh = qMax(1, qRound(rec.sigHPt * pxPerPt));
        QPixmap pm = QPixmap::fromImage(
            rec.image.scaled(sw, sh, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        auto* item = new SigSceneItem(pm);

        // Place: convert PDF bottom-left → scene top-left
        double sx = ix + rec.xPdf * pxPerPt;
        double sy = iy + (pageHPt - rec.yPdf - rec.sigHPt) * pxPerPt;
        item->setPos(sx, sy);

        m_scene->addItem(item);
        rec.item = item;
    }
}

void PdfViewer::addSigOverlay(const QImage& img, int pageIdx,
                               double xPdf, double yPdf,
                               double sigWPt, double sigHPt)
{
    SigRecord rec;
    rec.image   = img;
    rec.pageIdx = pageIdx;
    rec.xPdf    = xPdf;
    rec.yPdf    = yPdf;
    rec.sigWPt  = sigWPt;
    rec.sigHPt  = sigHPt;
    rec.item    = nullptr;
    m_sigRecords.append(rec);
    refreshSigItems();   // re-renders only the new item (harmless to redo all)
}

QList<SigCoords> PdfViewer::takePendingSignatures() {
    // Harvest final drag positions from all items
    for (auto& rec : m_sigRecords) harvestSigPosition(rec);

    QList<SigCoords> result;
    for (const auto& rec : m_sigRecords) {
        result.append({ rec.image, rec.pageIdx,
                        rec.xPdf, rec.yPdf, rec.sigWPt, rec.sigHPt });
    }
    // Remove from scene
    for (auto& rec : m_sigRecords) {
        if (rec.item) { m_scene->removeItem(rec.item); delete rec.item; }
    }
    m_sigRecords.clear();
    return result;
}

void PdfViewer::clearPendingSignatures() {
    for (auto& rec : m_sigRecords) {
        if (rec.item) { m_scene->removeItem(rec.item); delete rec.item; }
    }
    m_sigRecords.clear();
}
