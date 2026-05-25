#include "pagemanager.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <poppler-qt6.h>
#include <memory>

PageManager::PageManager(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    m_list = new QListWidget(this);
    m_list->setViewMode(QListWidget::IconMode);
    m_list->setIconSize(QSize(120,160));
    m_list->setResizeMode(QListWidget::Adjust);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setSpacing(4);
    m_list->setUniformItemSizes(true);

    layout->addWidget(m_list);

    connect(m_list, &QListWidget::itemActivated,
            this,   &PageManager::onItemActivated);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved,
            this,   [this](auto,auto,auto,auto,auto){ onDropEvent(); });
    connect(m_list, &QListWidget::customContextMenuRequested,
            this,   &PageManager::onContextMenu);
}

QPixmap PageManager::renderThumb(const QString& path, int pageIdx, int size) {
    auto doc = Poppler::Document::load(path);
    if (!doc || pageIdx >= doc->numPages()) return {};
    std::unique_ptr<Poppler::Page> pg(doc->page(pageIdx));
    if (!pg) return {};
    double dpi = size / (pg->pageSizeF().height() / 72.0);
    QImage img = pg->renderToImage(dpi, dpi);
    return QPixmap::fromImage(img).scaled(size, size*4/3,
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void PageManager::loadDocument(const QString& path, int count) {
    m_path  = path;
    m_count = count;
    buildThumbnails(path, count);
}

void PageManager::buildThumbnails(const QString& path, int count) {
    m_list->clear();
    for (int i = 0; i < count; ++i) {
        auto* item = new QListWidgetItem(
            QIcon(renderThumb(path, i)), QString("Page %1").arg(i+1));
        item->setData(Qt::UserRole, i);   // original index
        item->setSizeHint(QSize(130, 180));
        m_list->addItem(item);
    }
}

void PageManager::setCurrentPage(int index) {
    if (index >= 0 && index < m_list->count())
        m_list->setCurrentRow(index);
}

void PageManager::resetIndices() {
    for (int i = 0; i < m_list->count(); ++i) {
        m_list->item(i)->setData(Qt::UserRole, i);
        m_list->item(i)->setText(QString("Page %1").arg(i+1));
    }
}

void PageManager::onItemActivated(QListWidgetItem* item) {
    emit pageSelected(item->data(Qt::UserRole).toInt());
}

void PageManager::onDropEvent() {
    QVector<int> order;
    for (int i = 0; i < m_list->count(); ++i)
        order.append(m_list->item(i)->data(Qt::UserRole).toInt());
    emit orderChanged(order);
}

void PageManager::onContextMenu(const QPoint& pos) {
    auto* item = m_list->itemAt(pos);
    if (!item) return;
    int orig = item->data(Qt::UserRole).toInt();

    QMenu menu(this);
    auto* rotCW  = menu.addAction("Rotate CW");
    auto* rotCCW = menu.addAction("Rotate CCW");
    menu.addSeparator();
    auto* del = menu.addAction("Delete Page");

    auto* chosen = menu.exec(m_list->mapToGlobal(pos));
    if (chosen == rotCW)  emit pageRotated(orig,  90);
    if (chosen == rotCCW) emit pageRotated(orig, -90);
    if (chosen == del)    emit pageDeleted(orig);
}
