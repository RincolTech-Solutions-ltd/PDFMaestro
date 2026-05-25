#include "pdfviewer.h"
#include "annotationoverlay.h"
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QDebug>

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
}

PdfViewer::~PdfViewer() = default;

void PdfViewer::clear() {
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
    m_scene->clear();
    m_pageItems.clear();
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
}

void PdfViewer::rerenderAtZoom() {
    if (!m_doc) return;
    try {
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
