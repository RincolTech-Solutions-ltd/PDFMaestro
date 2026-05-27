#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QActionGroup>
#include <QImage>
#include "config.h"
#include "pdfviewer.h"
#include "pagemanager.h"
#include "searchbar.h"
#include "operations.h"
#include "toc.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    void openFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // File
    void onOpen();
    void onOpenRecent();
    void onSave();
    void onSaveAs();
    void onClose();

    // Edit / Page ops
    void onMerge();
    void onSplit();
    void onCrop();
    void onRotateCW();
    void onRotateCCW();
    void onDeletePage();

    // Tools
    void onInsertSignature();
    void onApplyRedactions();

    // View
    void onFindBar();
    void onFitWidth();
    void onFitPage();
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();

    // Settings
    void onSettings();
    void onAbout();

    // Internal wiring
    void onDocumentLoaded(const QString& path, int pages);
    void onPageChanged(int current, int total);
    void onZoomChanged(double zoom);
    void onAnnotation(const QVariantMap& payload);
    void onPageOrder(const QVector<int>& newOrder);
    void onPageDeleted(int origIdx);
    void onPageRotated(int origIdx, int degrees);
    void onMatchSelected(int pageIdx, const QRectF& pdfRect);
    void onTocItemActivated(QTreeWidgetItem* item, int col);

    void onSearchClosed();

private:
    void setupActions();
    void setupMenus();
    void setupToolbar();
    void setupDocks();
    void setupStatusBar();
    void buildTocTree(QTreeWidgetItem* parent, const QVector<TocEntry>& entries, int& idx);

    void loadFile(const QString& path);
    void reloadFromQpdf();
    void updateRecentMenu();
    void setModified(bool v);
    void pushPageCountToPageManager();
    void burnPendingSignatures();   // harvest pending sigs → QPDF just before Save

    // Core objects
    Config*      m_config;
    PdfViewer*   m_viewer;
    PageManager* m_pageManager;
    SearchBar*   m_searchBar;

    // QPDF state — all operations work on this
    QPDF         m_qpdf;
    bool         m_qpdfLoaded = false;
    QString      m_currentPath;
    bool         m_modified   = false;

    // Pending signature image (held while user is in drag-to-place mode)
    QImage       m_pendingSig;

    // TOC dock
    QDockWidget* m_tocDock;
    QTreeWidget* m_tocTree;

    // Page panel dock
    QDockWidget* m_pageDock;

    // Status bar
    QLabel*      m_statusPage;
    QLabel*      m_statusZoom;
    QLabel*      m_statusPath;

    // Actions
    QAction* m_actSave;
    QAction* m_actSaveAs;
    QAction* m_actClose;
    QAction* m_actRotateCW;
    QAction* m_actRotateCCW;
    QAction* m_actDeletePage;
    QAction* m_actSplit;
    QAction* m_actCrop;
    QAction* m_actInsertSig;
    QAction* m_actApplyRedact;
    QAction* m_actFind;
    QMenu*   m_recentMenu;
    QActionGroup* m_toolGroup;
};
