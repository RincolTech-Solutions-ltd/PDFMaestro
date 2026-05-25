#include "searchbar.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <poppler-page.h>

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("Find in document…");
    m_input->setMinimumWidth(200);

    m_caseSensitive = new QCheckBox("Aa", this);
    m_caseSensitive->setToolTip("Case sensitive");

    m_btnPrev  = new QPushButton("◀", this);
    m_btnNext  = new QPushButton("▶", this);
    m_btnClose = new QPushButton("✕", this);
    m_status   = new QLabel(this);

    for (auto* b : { m_btnPrev, m_btnNext, m_btnClose })
        b->setFixedWidth(28);

    layout->addWidget(m_input);
    layout->addWidget(m_caseSensitive);
    layout->addWidget(m_btnPrev);
    layout->addWidget(m_btnNext);
    layout->addWidget(m_status);
    layout->addStretch();
    layout->addWidget(m_btnClose);

    connect(m_input,   &QLineEdit::returnPressed,  this, &SearchBar::onSearch);
    connect(m_input,   &QLineEdit::textChanged,     this, [this](const QString& t){
        if (t != m_lastQuery) { m_matches.clear(); m_current = -1; updateStatus(); }
    });
    connect(m_btnPrev,  &QPushButton::clicked, this, &SearchBar::onPrev);
    connect(m_btnNext,  &QPushButton::clicked, this, &SearchBar::onNext);
    connect(m_btnClose, &QPushButton::clicked, this, &SearchBar::onClose);
    connect(m_caseSensitive, &QCheckBox::toggled, this, [this](){
        m_matches.clear(); m_current = -1;
    });

    installEventFilter(this);
    m_input->installEventFilter(this);
}

void SearchBar::setDocument(Poppler::Document* doc) {
    m_doc = doc;
    m_matches.clear();
    m_current  = -1;
    m_lastQuery.clear();
    updateStatus();
}

void SearchBar::focusInput() {
    m_input->setFocus();
    m_input->selectAll();
}

void SearchBar::onSearch() {
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    if (text != m_lastQuery || m_matches.isEmpty())
        runSearch(text);
    else
        onNext();
}

void SearchBar::runSearch(const QString& text) {
    m_matches.clear();
    m_current = -1;
    m_lastQuery = text;

    if (!m_doc || text.isEmpty()) { updateStatus(); return; }

    Poppler::Page::SearchFlags flags = Poppler::Page::NoSearchFlags;
    if (m_caseSensitive->isChecked())
        flags |= Poppler::Page::IgnoreCase;  // inverted: IgnoreCase means NOT case-sensitive

    for (int i = 0; i < m_doc->numPages(); ++i) {
        std::unique_ptr<Poppler::Page> pg(m_doc->page(i));
        if (!pg) continue;
        QList<QRectF> rects = pg->search(text, flags);
        for (const auto& r : rects)
            m_matches.append({ i, r });
    }

    updateStatus();
    if (!m_matches.isEmpty()) {
        m_current = 0;
        showCurrent();
    }
}

void SearchBar::onNext() {
    if (m_matches.isEmpty()) { onSearch(); return; }
    m_current = (m_current + 1) % m_matches.size();
    showCurrent();
    updateStatus();
}

void SearchBar::onPrev() {
    if (m_matches.isEmpty()) { onSearch(); return; }
    m_current = (m_current - 1 + m_matches.size()) % m_matches.size();
    showCurrent();
    updateStatus();
}

void SearchBar::showCurrent() {
    if (m_current < 0 || m_current >= m_matches.size()) return;
    const auto& m = m_matches[m_current];
    emit matchSelected(m.pageIndex, m.rect);
    updateStatus();
}

void SearchBar::updateStatus() {
    if (m_matches.isEmpty() && !m_lastQuery.isEmpty())
        m_status->setText("No results");
    else if (!m_matches.isEmpty())
        m_status->setText(QString("%1 / %2").arg(m_current + 1).arg(m_matches.size()));
    else
        m_status->clear();
}

void SearchBar::onClose() {
    m_input->clear();
    m_matches.clear();
    m_current  = -1;
    m_lastQuery.clear();
    updateStatus();
    emit closed();
}
