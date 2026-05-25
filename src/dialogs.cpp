#include "dialogs.h"
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QLabel>

// ── MergeDialog ───────────────────────────────────────────────────────────────

MergeDialog::MergeDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Merge PDFs");
    setMinimumSize(420, 320);

    auto* root   = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);

    auto* btnRow    = new QHBoxLayout;
    auto* btnAdd    = new QPushButton("Add PDF…");
    auto* btnRemove = new QPushButton("Remove");
    auto* btnUp     = new QPushButton("▲ Up");
    auto* btnDown   = new QPushButton("▼ Down");
    btnRow->addWidget(btnAdd);
    btnRow->addWidget(btnRemove);
    btnRow->addWidget(btnUp);
    btnRow->addWidget(btnDown);
    btnRow->addStretch();

    connect(btnAdd,    &QPushButton::clicked, this, &MergeDialog::onAdd);
    connect(btnRemove, &QPushButton::clicked, this, &MergeDialog::onRemove);
    connect(btnUp,     &QPushButton::clicked, this, &MergeDialog::onMoveUp);
    connect(btnDown,   &QPushButton::clicked, this, &MergeDialog::onMoveDown);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addLayout(btnRow);
    root->addWidget(m_list, 1);
    root->addWidget(btns);
}

QStringList MergeDialog::selectedFiles() const {
    QStringList out;
    for (int i = 0; i < m_list->count(); ++i)
        out << m_list->item(i)->data(Qt::UserRole).toString();
    return out;
}

void MergeDialog::onAdd() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Select PDFs", {}, "PDF files (*.pdf)");
    for (const auto& f : files) {
        auto* it = new QListWidgetItem(QFileInfo(f).fileName());
        it->setData(Qt::UserRole, f);
        m_list->addItem(it);
    }
}

void MergeDialog::onRemove() {
    qDeleteAll(m_list->selectedItems());
}

void MergeDialog::onMoveUp() {
    int row = m_list->currentRow();
    if (row <= 0) return;
    auto* it = m_list->takeItem(row);
    m_list->insertItem(row - 1, it);
    m_list->setCurrentRow(row - 1);
}

void MergeDialog::onMoveDown() {
    int row = m_list->currentRow();
    if (row < 0 || row >= m_list->count() - 1) return;
    auto* it = m_list->takeItem(row);
    m_list->insertItem(row + 1, it);
    m_list->setCurrentRow(row + 1);
}

// ── SplitDialog ───────────────────────────────────────────────────────────────

SplitDialog::SplitDialog(int totalPages, QWidget* parent)
    : QDialog(parent), m_totalPages(totalPages)
{
    setWindowTitle("Split PDF");
    setMinimumWidth(380);

    auto* root   = new QVBoxLayout(this);
    auto* grp    = new QGroupBox("Split mode", this);
    auto* grpLay = new QVBoxLayout(grp);

    auto* rByRanges  = new QRadioButton("By page ranges");
    auto* rEveryN    = new QRadioButton("Every N pages");
    auto* rEachPage  = new QRadioButton("Each page as separate file");
    rByRanges->setChecked(true);

    auto* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(rByRanges,  0);
    modeGroup->addButton(rEveryN,    1);
    modeGroup->addButton(rEachPage,  2);

    // Ranges widget
    m_rangesWidget = new QWidget;
    auto* rfl   = new QFormLayout(m_rangesWidget);
    m_ranges = new QLineEdit;
    m_ranges->setPlaceholderText(QString("e.g. 1-3,4-%1").arg(totalPages));
    rfl->addRow("Ranges:", m_ranges);
    rfl->addRow(new QLabel(QString("(Total pages: %1)").arg(totalPages)));

    // EveryN widget
    m_everyNWidget = new QWidget;
    auto* enl = new QFormLayout(m_everyNWidget);
    m_n = new QSpinBox;
    m_n->setRange(1, qMax(1, totalPages - 1));
    m_n->setValue(1);
    enl->addRow("Pages per chunk:", m_n);

    auto* stack = new QStackedWidget(this);
    stack->addWidget(m_rangesWidget);
    stack->addWidget(m_everyNWidget);
    stack->addWidget(new QLabel("Each page saved as Page_1.pdf, Page_2.pdf…"));

    connect(modeGroup, &QButtonGroup::idClicked, stack, &QStackedWidget::setCurrentIndex);

    grpLay->addWidget(rByRanges);
    grpLay->addWidget(rEveryN);
    grpLay->addWidget(rEachPage);
    grpLay->addWidget(stack);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(grp);
    root->addWidget(btns);

    // keep for mode() query
    m_modeEachPage = new QCheckBox; m_modeEachPage->setVisible(false); m_modeEachPage->setParent(this);
    m_modeEveryN   = new QCheckBox; m_modeEveryN->setVisible(false);   m_modeEveryN->setParent(this);
    connect(rEveryN,   &QRadioButton::toggled, m_modeEveryN,   &QCheckBox::setChecked);
    connect(rEachPage, &QRadioButton::toggled, m_modeEachPage, &QCheckBox::setChecked);
}

SplitDialog::Mode SplitDialog::mode() const {
    if (m_modeEachPage->isChecked()) return EachPage;
    if (m_modeEveryN->isChecked())   return EveryN;
    return ByRanges;
}

QString SplitDialog::rangeText() const { return m_ranges->text(); }
int     SplitDialog::everyN()    const { return m_n->value(); }

// ── CropDialog ────────────────────────────────────────────────────────────────

static QDoubleSpinBox* makeMarginSpin() {
    auto* s = new QDoubleSpinBox;
    s->setRange(0, 500);
    s->setSingleStep(1.0);
    s->setSuffix(" pt");
    return s;
}

CropDialog::CropDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Crop Page");
    setMinimumWidth(300);

    auto* root   = new QVBoxLayout(this);
    auto* form   = new QFormLayout;

    m_top    = makeMarginSpin();
    m_bottom = makeMarginSpin();
    m_left   = makeMarginSpin();
    m_right  = makeMarginSpin();
    form->addRow("Top margin (pt):",    m_top);
    form->addRow("Bottom margin (pt):", m_bottom);
    form->addRow("Left margin (pt):",   m_left);
    form->addRow("Right margin (pt):",  m_right);

    m_allPages = new QCheckBox("Apply to all pages", this);
    form->addRow(m_allPages);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addLayout(form);
    root->addWidget(btns);
}

double CropDialog::top()      const { return m_top->value();    }
double CropDialog::bottom()   const { return m_bottom->value(); }
double CropDialog::left()     const { return m_left->value();   }
double CropDialog::right()    const { return m_right->value();  }
bool   CropDialog::allPages() const { return m_allPages->isChecked(); }
