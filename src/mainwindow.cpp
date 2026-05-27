#include "mainwindow.h"
#include "toc.h"
#include "annotations.h"
#include "dialogs.h"
#include "signaturedialog.h"
#include "settingsdialog.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QFileInfo>
#include <QPainter>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>

// ── Construction ──────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("PDFMaestro");
    setMinimumSize(900, 640);
    setAcceptDrops(true);

    m_config = new Config(this);

    // Central: viewer + search bar stacked vertically
    auto* central = new QWidget(this);
    auto* cl      = new QVBoxLayout(central);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    m_viewer    = new PdfViewer(central);
    m_searchBar = new SearchBar(central);
    m_searchBar->setVisible(false);

    cl->addWidget(m_searchBar);
    cl->addWidget(m_viewer, 1);
    setCentralWidget(central);

    m_pageManager = new PageManager(this);

    setupDocks();
    setupActions();
    setupMenus();
    setupToolbar();
    setupStatusBar();

    // Viewer signals
    connect(m_viewer, &PdfViewer::documentLoaded, this, &MainWindow::onDocumentLoaded);
    connect(m_viewer, &PdfViewer::pageChanged,    this, &MainWindow::onPageChanged);
    connect(m_viewer, &PdfViewer::zoomChanged,    this, &MainWindow::onZoomChanged);
    connect(m_viewer, &PdfViewer::annotationCommitted, this, &MainWindow::onAnnotation);
    // Restore pointer tool in toolbar when signature placement ends (placed or Esc)
    connect(m_viewer, &PdfViewer::signatureCancelled, this, [this](){
        for (auto* a : m_toolGroup->actions()) {
            if (a->data().toString() == "pointer") { a->setChecked(true); break; }
        }
        m_pendingSig = QImage();
        statusBar()->clearMessage();
    });

    // PageManager signals
    connect(m_pageManager, &PageManager::pageSelected, m_viewer,      &PdfViewer::goToPage);
    connect(m_pageManager, &PageManager::orderChanged, this,          &MainWindow::onPageOrder);
    connect(m_pageManager, &PageManager::pageDeleted,  this,          &MainWindow::onPageDeleted);
    connect(m_pageManager, &PageManager::pageRotated,  this,          &MainWindow::onPageRotated);

    // Search bar
    connect(m_searchBar, &SearchBar::matchSelected, this, &MainWindow::onMatchSelected);
    connect(m_searchBar, &SearchBar::closed,        this, &MainWindow::onSearchClosed);

    // TOC
    connect(m_tocTree, &QTreeWidget::itemActivated, this, &MainWindow::onTocItemActivated);

    // Restore last document
    if (m_config->restoreLastDocument()) {
        const auto recents = m_config->recentFiles();
        if (!recents.isEmpty()) loadFile(recents.first());
    }

    restoreGeometry(m_config->windowGeometry());
    restoreState(m_config->windowState());
}

// ── Setup helpers ─────────────────────────────────────────────────────────────

void MainWindow::setupDocks() {
    // TOC dock
    m_tocDock = new QDockWidget("Table of Contents", this);
    m_tocDock->setObjectName("toc_dock");
    m_tocTree = new QTreeWidget;
    m_tocTree->setHeaderHidden(true);
    m_tocTree->setColumnCount(2);
    m_tocTree->setColumnWidth(0, 180);
    m_tocDock->setWidget(m_tocTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_tocDock);
    m_tocDock->setVisible(m_config->showTocPanel());

    // Page panel dock
    m_pageDock = new QDockWidget("Pages", this);
    m_pageDock->setObjectName("page_dock");
    m_pageDock->setWidget(m_pageManager);
    addDockWidget(Qt::RightDockWidgetArea, m_pageDock);
    m_pageDock->setVisible(m_config->showPagePanel());
}

void MainWindow::setupActions() {
    m_actSave        = new QAction(QIcon::fromTheme("document-save"),       "Save",               this);
    m_actSaveAs      = new QAction(QIcon::fromTheme("document-save-as"),    "Save As…",           this);
    m_actClose       = new QAction(QIcon::fromTheme("document-close"),      "Close",              this);
    m_actRotateCW    = new QAction(QIcon::fromTheme("object-rotate-right"), "Rotate CW",          this);
    m_actRotateCCW   = new QAction(QIcon::fromTheme("object-rotate-left"),  "Rotate CCW",         this);
    m_actDeletePage  = new QAction(QIcon::fromTheme("edit-delete"),         "Delete Current Page",this);
    m_actSplit       = new QAction("Split PDF…",          this);
    m_actCrop        = new QAction("Crop Page…",          this);
    m_actInsertSig   = new QAction("Insert Signature…",   this);
    m_actApplyRedact = new QAction("Apply Redactions",    this);
    m_actFind        = new QAction(QIcon::fromTheme("edit-find"), "Find…",  this);

    m_actSave->setShortcut(QKeySequence::Save);
    m_actSaveAs->setShortcut(QKeySequence::SaveAs);
    m_actFind->setShortcut(QKeySequence::Find);
    m_actRotateCW->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_actRotateCCW->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

    // Annotation tool group
    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    auto makeToolAction = [&](const QString& icon, const QString& label, const QString& toolId) {
        auto* act = new QAction(QIcon::fromTheme(icon), label, this);
        act->setCheckable(true);
        act->setData(toolId);
        m_toolGroup->addAction(act);
        return act;
    };
    makeToolAction("cursor",         "Pointer",   "pointer"  )->setChecked(true);
    makeToolAction("draw-highlight", "Highlight", "highlight");
    makeToolAction("insert-text",    "Note",      "note"     );
    makeToolAction("draw-freehand",  "Ink",       "ink"      );
    makeToolAction("stamp",          "Stamp",     "stamp"    );
    makeToolAction("edit-clear",     "Redact",    "redact"   );

    connect(m_toolGroup, &QActionGroup::triggered, this, [this](QAction* act){
        m_viewer->setAnnotationTool(act->data().toString());
    });

    connect(m_actSave,        &QAction::triggered, this, &MainWindow::onSave);
    connect(m_actSaveAs,      &QAction::triggered, this, &MainWindow::onSaveAs);
    connect(m_actClose,       &QAction::triggered, this, &MainWindow::onClose);
    connect(m_actRotateCW,    &QAction::triggered, this, &MainWindow::onRotateCW);
    connect(m_actRotateCCW,   &QAction::triggered, this, &MainWindow::onRotateCCW);
    connect(m_actDeletePage,  &QAction::triggered, this, &MainWindow::onDeletePage);
    connect(m_actSplit,       &QAction::triggered, this, &MainWindow::onSplit);
    connect(m_actCrop,        &QAction::triggered, this, &MainWindow::onCrop);
    connect(m_actInsertSig,   &QAction::triggered, this, &MainWindow::onInsertSignature);
    connect(m_actApplyRedact, &QAction::triggered, this, &MainWindow::onApplyRedactions);
    connect(m_actFind,        &QAction::triggered, this, &MainWindow::onFindBar);
}

void MainWindow::setupMenus() {
    // File
    auto* file = menuBar()->addMenu("&File");
    auto* open = new QAction(QIcon::fromTheme("document-open"), "&Open…", this);
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &MainWindow::onOpen);
    file->addAction(open);
    m_recentMenu = file->addMenu("Open &Recent");
    file->addSeparator();
    file->addAction(m_actSave);
    file->addAction(m_actSaveAs);
    file->addSeparator();
    file->addAction(m_actClose);
    file->addSeparator();
    auto* quit = new QAction("&Quit", this);
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);
    file->addAction(quit);
    updateRecentMenu();

    // Edit
    auto* edit = menuBar()->addMenu("&Edit");
    edit->addAction(m_actRotateCW);
    edit->addAction(m_actRotateCCW);
    edit->addAction(m_actDeletePage);
    edit->addSeparator();
    auto* actMerge = new QAction("&Merge PDFs…", this);
    connect(actMerge, &QAction::triggered, this, &MainWindow::onMerge);
    edit->addAction(actMerge);
    edit->addAction(m_actSplit);
    edit->addAction(m_actCrop);

    // Tools
    auto* tools = menuBar()->addMenu("&Tools");
    for (auto* a : m_toolGroup->actions()) tools->addAction(a);
    tools->addSeparator();
    tools->addAction(m_actInsertSig);
    tools->addAction(m_actApplyRedact);

    // View
    auto* view = menuBar()->addMenu("&View");
    view->addAction(m_actFind);
    view->addSeparator();
    auto* actZoomIn = new QAction("Zoom &In", this);
    actZoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(actZoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);
    view->addAction(actZoomIn);
    auto* actZoomOut = new QAction("Zoom &Out", this);
    actZoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(actZoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);
    view->addAction(actZoomOut);
    auto* actZoomReset = new QAction("&Reset Zoom", this);
    actZoomReset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(actZoomReset, &QAction::triggered, this, &MainWindow::onZoomReset);
    view->addAction(actZoomReset);
    view->addSeparator();
    auto* actFitW = new QAction("Fit &Width", this);
    connect(actFitW, &QAction::triggered, this, &MainWindow::onFitWidth);
    view->addAction(actFitW);
    auto* actFitP = new QAction("Fit &Page", this);
    connect(actFitP, &QAction::triggered, this, &MainWindow::onFitPage);
    view->addAction(actFitP);
    view->addSeparator();
    view->addAction(m_tocDock->toggleViewAction());
    view->addAction(m_pageDock->toggleViewAction());

    // Settings
    auto* settings = menuBar()->addMenu("&Settings");
    settings->addAction("&Preferences…", this, &MainWindow::onSettings);
    settings->addSeparator();
    settings->addAction("&About PDFMaestro", this, &MainWindow::onAbout);
}

void MainWindow::setupToolbar() {
    auto* tb = addToolBar("Main");
    tb->setObjectName("main_toolbar");
    tb->setMovable(false);

    tb->addAction(QIcon::fromTheme("document-open"),  "Open",   this, &MainWindow::onOpen);
    tb->addAction(m_actSave);
    tb->addSeparator();
    tb->addAction(m_actRotateCW);
    tb->addAction(m_actRotateCCW);
    tb->addAction(m_actDeletePage);
    tb->addSeparator();
    for (auto* a : m_toolGroup->actions()) tb->addAction(a);
    tb->addSeparator();
    tb->addAction(m_actFind);
}

void MainWindow::setupStatusBar() {
    m_statusPage = new QLabel("No document");
    m_statusZoom = new QLabel;
    m_statusPath = new QLabel;
    statusBar()->addWidget(m_statusPage);
    statusBar()->addPermanentWidget(m_statusZoom);
    statusBar()->addPermanentWidget(m_statusPath);
}

// ── File operations ───────────────────────────────────────────────────────────

void MainWindow::openFile(const QString& path) { loadFile(path); }

void MainWindow::onOpen() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open PDF", m_config->lastOpenDir(), "PDF files (*.pdf)");
    if (!path.isEmpty()) {
        m_config->setLastOpenDir(QFileInfo(path).dir().absolutePath());
        loadFile(path);
    }
}

void MainWindow::onOpenRecent() {
    if (auto* act = qobject_cast<QAction*>(sender()))
        loadFile(act->data().toString());
}

void MainWindow::loadFile(const QString& path) {
    // Discard any pending sigs from a previous document (don't burn them)
    m_viewer->clearPendingSignatures();
    m_pendingSig = QImage();
    try {
        m_qpdf.processFile(path.toStdString().c_str());
        m_qpdfLoaded = true;
        m_currentPath = path;
        m_modified = false;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Error", "Cannot read file: " + path);
            return;
        }
        m_viewer->loadFromBytes(f.readAll(), path);
        f.close();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Open Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::reloadFromQpdf() {
    if (!m_qpdfLoaded) return;
    QByteArray bytes = Operations::toBytes(m_qpdf);
    m_viewer->loadFromBytes(bytes, m_currentPath);
    pushPageCountToPageManager();
}

void MainWindow::pushPageCountToPageManager() {
    if (!m_qpdfLoaded) return;
    m_pageManager->loadDocument(m_currentPath, m_viewer->pageCount());
}

void MainWindow::onDocumentLoaded(const QString& path, int pages) {
    m_config->addRecentFile(path);
    updateRecentMenu();
    m_statusPath->setText(QFileInfo(path).fileName());
    setWindowTitle(QString("PDFMaestro — %1").arg(QFileInfo(path).fileName()));
    pushPageCountToPageManager();

    // Build TOC
    m_tocTree->clear();
    if (m_viewer->document()) {
        auto entries = readToc(m_viewer->document());
        int idx = 0;
        buildTocTree(nullptr, entries, idx);
    }

    // Feed document to search
    m_searchBar->setDocument(m_viewer->document());
    m_actSave->setEnabled(true);
    m_actSaveAs->setEnabled(true);
    Q_UNUSED(pages)
}

void MainWindow::buildTocTree(QTreeWidgetItem* parent, const QVector<TocEntry>& entries, int& idx) {
    while (idx < entries.size()) {
        const TocEntry& e = entries[idx];
        QStringList cols = { e.title, QString::number(e.pageIndex + 1) };
        QTreeWidgetItem* item = parent
            ? new QTreeWidgetItem(parent, cols)
            : new QTreeWidgetItem(m_tocTree, cols);
        item->setData(0, Qt::UserRole, e.pageIndex);
        ++idx;
        if (idx < entries.size() && entries[idx].level > e.level)
            buildTocTree(item, entries, idx);
        if (idx < entries.size() && entries[idx].level < e.level)
            break;
    }
    m_tocTree->expandAll();
}

void MainWindow::onTocItemActivated(QTreeWidgetItem* item, int) {
    if (!item) return;
    int pg = item->data(0, Qt::UserRole).toInt();
    m_viewer->goToPage(pg);
}

void MainWindow::burnPendingSignatures() {
    if (!m_qpdfLoaded) return;
    const auto sigs = m_viewer->takePendingSignatures();
    if (sigs.isEmpty()) return;
    try {
        for (const auto& s : sigs)
            Operations::overlayImageOnPage(m_qpdf, s.page, s.image,
                                           s.sigW, s.sigH, s.x, s.y);
        // Reload viewer so the burned-in sigs render via Poppler
        reloadFromQpdf();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Signature Error",
                              QString("Failed to commit signature: %1").arg(e.what()));
    }
}

void MainWindow::onSave() {
    if (m_currentPath.isEmpty()) { onSaveAs(); return; }
    if (!m_qpdfLoaded) return;
    burnPendingSignatures();   // commit any draggable overlays to QPDF first
    try {
        QPDFWriter w(m_qpdf, m_currentPath.toStdString().c_str());
        w.write();
        setModified(false);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::onSaveAs() {
    if (!m_qpdfLoaded) return;
    const QString path = QFileDialog::getSaveFileName(
        this, "Save PDF As", m_currentPath, "PDF files (*.pdf)");
    if (path.isEmpty()) return;
    burnPendingSignatures();   // commit overlays before writing
    try {
        QPDFWriter w(m_qpdf, path.toStdString().c_str());
        w.write();
        m_currentPath = path;
        setModified(false);
        m_statusPath->setText(QFileInfo(path).fileName());
        setWindowTitle(QString("PDFMaestro — %1").arg(QFileInfo(path).fileName()));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save Error", QString::fromStdString(e.what()));
    }
}

void MainWindow::onClose() {
    if (m_modified) {
        auto ans = QMessageBox::question(this, "Unsaved Changes",
            "Save changes before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ans == QMessageBox::Cancel) return;
        if (ans == QMessageBox::Save) onSave();
    }
    m_qpdfLoaded = false;
    m_currentPath.clear();
    m_modified = false;
    m_pendingSig = QImage();
    m_viewer->clear();   // also calls clearPendingSignatures()
    m_pageManager->clear();
    m_tocTree->clear();
    m_searchBar->setDocument(nullptr);
    m_statusPage->setText("No document");
    m_statusZoom->clear();
    m_statusPath->clear();
    setWindowTitle("PDFMaestro");
}

void MainWindow::updateRecentMenu() {
    m_recentMenu->clear();
    for (const auto& f : m_config->recentFiles()) {
        auto* act = m_recentMenu->addAction(QFileInfo(f).fileName());
        act->setData(f);
        act->setToolTip(f);
        connect(act, &QAction::triggered, this, &MainWindow::onOpenRecent);
    }
    if (m_config->recentFiles().isEmpty())
        m_recentMenu->addAction("(empty)")->setEnabled(false);
    m_recentMenu->addSeparator();
    m_recentMenu->addAction("Clear Recent", this, [this](){
        m_config->clearRecentFiles(); updateRecentMenu();
    });
}

void MainWindow::setModified(bool v) {
    m_modified = v;
    setWindowModified(v);
}

// ── Page operations ───────────────────────────────────────────────────────────

void MainWindow::onRotateCW() {
    if (!m_qpdfLoaded) return;
    try {
        Operations::rotatePage(m_qpdf, m_viewer->currentPage(), 90);
        setModified(true);
        reloadFromQpdf();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Error", e.what()); }
}

void MainWindow::onRotateCCW() {
    if (!m_qpdfLoaded) return;
    try {
        Operations::rotatePage(m_qpdf, m_viewer->currentPage(), -90);
        setModified(true);
        reloadFromQpdf();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Error", e.what()); }
}

void MainWindow::onDeletePage() {
    if (!m_qpdfLoaded) return;
    if (QMessageBox::question(this, "Delete Page",
            QString("Delete page %1?").arg(m_viewer->currentPage() + 1))
        != QMessageBox::Yes) return;
    try {
        Operations::deletePages(m_qpdf, { m_viewer->currentPage() });
        setModified(true);
        reloadFromQpdf();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Error", e.what()); }
}

void MainWindow::onMerge() {
    MergeDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QStringList files = dlg.selectedFiles();
    if (files.isEmpty()) return;

    // If no doc is open, use first file as base
    if (!m_qpdfLoaded) {
        loadFile(files.takeFirst());
        if (!m_qpdfLoaded) return;
    }
    try {
        for (const auto& f : files) {
            QPDF src;
            src.processFile(f.toStdString().c_str());
            Operations::mergeInto(m_qpdf, src);
        }
        setModified(true);
        reloadFromQpdf();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Merge Error", e.what()); }
}

void MainWindow::onSplit() {
    if (!m_qpdfLoaded) return;
    SplitDialog dlg(m_viewer->pageCount(), this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString outDir = QFileDialog::getExistingDirectory(this, "Output Folder");
    if (outDir.isEmpty()) return;

    const QString base = QFileInfo(m_currentPath).baseName();
    try {
        QVector<Operations::Range> ranges;
        if (dlg.mode() == SplitDialog::ByRanges) {
            ranges = Operations::parseRanges(dlg.rangeText(), m_viewer->pageCount());
        } else if (dlg.mode() == SplitDialog::EveryN) {
            int n = dlg.everyN();
            for (int i = 0; i < m_viewer->pageCount(); i += n)
                ranges.append({i, qMin(i + n - 1, m_viewer->pageCount() - 1)});
        } else {
            for (int i = 0; i < m_viewer->pageCount(); ++i)
                ranges.append({i, i});
        }

        auto parts = Operations::splitByRanges(m_qpdf, ranges, outDir, base);
        QMessageBox::information(this, "Split", QString("Split into %1 file(s).").arg(parts.size()));
    } catch (const std::exception& e) { QMessageBox::critical(this, "Split Error", e.what()); }
}

void MainWindow::onCrop() {
    if (!m_qpdfLoaded) return;
    CropDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    try {
        if (dlg.allPages())
            Operations::cropAllPages(m_qpdf, dlg.left(), dlg.bottom(), dlg.right(), dlg.top());
        else
            Operations::cropPage(m_qpdf, m_viewer->currentPage(), dlg.left(), dlg.bottom(), dlg.right(), dlg.top());
        setModified(true);
        reloadFromQpdf();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Crop Error", e.what()); }
}

// ── Tools ─────────────────────────────────────────────────────────────────────

void MainWindow::onInsertSignature() {
    if (!m_qpdfLoaded) return;
    SignatureDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QImage sig = dlg.result();
    if (sig.isNull()) return;

    // Enter drag-to-place mode: ghost follows cursor, click commits position
    m_pendingSig = sig;
    m_viewer->beginSignaturePlacement(sig);
    statusBar()->showMessage("Move cursor to desired position  •  Click to place  •  Esc to cancel", 0);
}

void MainWindow::onApplyRedactions() {
    if (!m_qpdfLoaded) return;
    if (QMessageBox::question(this, "Apply Redactions",
            "This will permanently burn all redaction boxes into the PDF. Continue?")
        != QMessageBox::Yes) return;
    try {
        for (int i = 0; i < m_viewer->pageCount(); ++i)
            Annotations::applyRedactions(m_qpdf, i);
        setModified(true);
        reloadFromQpdf();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Redact Error", e.what()); }
}

// ── View ──────────────────────────────────────────────────────────────────────

void MainWindow::onFindBar() {
    m_searchBar->setVisible(true);
    m_searchBar->focusInput();
}

void MainWindow::onSearchClosed() {
    m_searchBar->setVisible(false);
    m_viewer->setFocus();
}

void MainWindow::onMatchSelected(int pageIdx, const QRectF&) {
    m_viewer->goToPage(pageIdx);
}

void MainWindow::onFitWidth()  { m_viewer->fitWidth(); }
void MainWindow::onFitPage()   { m_viewer->fitPage();  }
void MainWindow::onZoomIn()    { m_viewer->zoomIn();   }
void MainWindow::onZoomOut()   { m_viewer->zoomOut();  }
void MainWindow::onZoomReset() { m_viewer->zoomReset();}

// ── Settings ──────────────────────────────────────────────────────────────────

void MainWindow::onSettings() {
    SettingsDialog dlg(m_config, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_tocDock->setVisible(m_config->showTocPanel());
        m_pageDock->setVisible(m_config->showPagePanel());
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About PDFMaestro",
        "<b>PDFMaestro</b><br>"
        "Version 0.1.0<br><br>"
        "PDF viewer, editor, and annotation tool.<br>"
        "Built with Qt6, Poppler-Qt6, and QPDF.");
}

// ── Wiring ─────────────────────────────────────────────────────────────────────

void MainWindow::onPageChanged(int current, int total) {
    m_statusPage->setText(QString("Page %1 / %2").arg(current).arg(total));
    m_pageManager->setCurrentPage(current - 1);
}

void MainWindow::onZoomChanged(double zoom) {
    m_statusZoom->setText(QString("%1%").arg(qRound(zoom * 100)));
}

void MainWindow::onAnnotation(const QVariantMap& payload) {
    if (!m_qpdfLoaded) return;
    try {
        const QString type = payload["type"].toString();
        int pg = payload["page"].toInt();

        if (type == "highlight") {
            double x0 = payload["x0"].toDouble(), y0 = payload["y0"].toDouble();
            double x1 = payload["x1"].toDouble(), y1 = payload["y1"].toDouble();
            Annotations::Quad q{ x0,y0, x1,y0, x1,y1, x0,y1 };
            Annotations::addHighlight(m_qpdf, pg, { q });
        }
        else if (type == "note")
            Annotations::addTextNote(m_qpdf, pg,
                payload["x"].toDouble(), payload["y"].toDouble(),
                payload["contents"].toString());
        else if (type == "ink") {
            QVector<QVector<QPointF>> strokes;
            for (const auto& sv : payload["strokes"].toList()) {
                QVector<QPointF> s;
                for (const auto& pv : sv.toList())
                    s << pv.toPointF();
                strokes << s;
            }
            Annotations::addInk(m_qpdf, pg, strokes);
        }
        else if (type == "stamp")
            Annotations::addStamp(m_qpdf, pg,
                payload["x"].toDouble(), payload["y"].toDouble(),
                payload["w"].toDouble(), payload["h"].toDouble(),
                payload["name"].toString());
        else if (type == "redact")
            Annotations::addRedact(m_qpdf, pg,
                payload["x0"].toDouble(), payload["y0"].toDouble(),
                payload["x1"].toDouble(), payload["y1"].toDouble());
        else if (type == "signature") {
            // Don't burn to QPDF yet — add as a moveable scene overlay.
            // It gets burned when the user explicitly saves.
            if (!m_pendingSig.isNull()) {
                m_viewer->addSigOverlay(
                    m_pendingSig, pg,
                    payload["x"].toDouble(),    payload["y"].toDouble(),
                    payload["sigW"].toDouble(), payload["sigH"].toDouble());
            }
            m_pendingSig = QImage();
            statusBar()->showMessage(
                "Signature placed — drag to reposition, Delete to remove, Save to commit", 4000);
            setModified(true);
            return;   // skip reloadFromQpdf — overlay is already visible in scene
        }

        setModified(true);
        // For non-signature annotations reload the viewer
        reloadFromQpdf();
    } catch (const std::exception& e) {
        qWarning() << "Annotation error:" << e.what();
    }
}

void MainWindow::onPageOrder(const QVector<int>& newOrder) {
    if (!m_qpdfLoaded) return;
    try {
        Operations::applyPageOrder(m_qpdf, newOrder);
        setModified(true);
        reloadFromQpdf();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Error", e.what()); }
}

void MainWindow::onPageDeleted(int origIdx) {
    if (!m_qpdfLoaded) return;
    try {
        Operations::deletePages(m_qpdf, { origIdx });
        setModified(true);
        reloadFromQpdf();
        m_pageManager->resetIndices();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Error", e.what()); }
}

void MainWindow::onPageRotated(int origIdx, int degrees) {
    if (!m_qpdfLoaded) return;
    try {
        Operations::rotatePage(m_qpdf, origIdx, degrees);
        setModified(true);
        reloadFromQpdf();
    } catch (const std::exception& e) { QMessageBox::critical(this, "Error", e.what()); }
}

// ── Window events ─────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_modified) {
        auto ans = QMessageBox::question(this, "Unsaved Changes",
            "Save changes before quitting?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ans == QMessageBox::Cancel) { event->ignore(); return; }
        if (ans == QMessageBox::Save)   onSave();
    }
    m_config->setWindowGeometry(saveGeometry());
    m_config->setWindowState(saveState());
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const auto& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && url.toLocalFile().endsWith(".pdf", Qt::CaseInsensitive)) {
            loadFile(url.toLocalFile());
            break;
        }
    }
}
