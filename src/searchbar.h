#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <poppler-qt6.h>
#include <memory>

struct SearchMatch {
    int   pageIndex;
    QRectF rect;   // in PDF user space (points)
};

class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr);

    void setDocument(Poppler::Document* doc);  // non-owning

signals:
    void matchSelected(int pageIndex, const QRectF& pdfRect);
    void closed();

public slots:
    void focusInput();

private slots:
    void onSearch();
    void onNext();
    void onPrev();
    void onClose();

private:
    void runSearch(const QString& text);
    void showCurrent();
    void updateStatus();

    Poppler::Document* m_doc = nullptr;

    QLineEdit*   m_input;
    QPushButton* m_btnPrev;
    QPushButton* m_btnNext;
    QPushButton* m_btnClose;
    QLabel*      m_status;
    QCheckBox*   m_caseSensitive;

    QVector<SearchMatch> m_matches;
    int                  m_current = -1;
    QString              m_lastQuery;
};
