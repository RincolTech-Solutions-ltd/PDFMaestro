#pragma once
#include <QWidget>
#include <QListWidget>

class PageManager : public QWidget {
    Q_OBJECT
public:
    explicit PageManager(QWidget* parent = nullptr);

    void loadDocument(const QString& path, int count);
    void setCurrentPage(int index);
    void resetIndices();
    void clear() { m_list->clear(); m_path.clear(); m_count = 0; }

signals:
    void pageSelected(int index);
    void orderChanged(const QVector<int>& newOrder);
    void pageDeleted(int originalIndex);
    void pageRotated(int originalIndex, int degrees);

private slots:
    void onItemActivated(QListWidgetItem* item);
    void onDropEvent();
    void onContextMenu(const QPoint& pos);

private:
    void buildThumbnails(const QString& path, int count);
    QPixmap renderThumb(const QString& path, int pageIdx, int size = 120);

    QListWidget* m_list;
    QString      m_path;
    int          m_count = 0;
};
