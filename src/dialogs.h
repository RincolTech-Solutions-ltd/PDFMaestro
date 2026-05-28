#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QColor>
#include <QPushButton>

// ── MergeDialog ───────────────────────────────────────────────────────────────
// Lets the user build an ordered list of PDF files to merge.
class MergeDialog : public QDialog {
    Q_OBJECT
public:
    explicit MergeDialog(QWidget* parent = nullptr);
    QStringList selectedFiles() const;

private slots:
    void onAdd();
    void onRemove();
    void onMoveUp();
    void onMoveDown();

private:
    QListWidget* m_list;
};

// ── SplitDialog ───────────────────────────────────────────────────────────────
// Two modes: split by page ranges (e.g. "1-3,4-6") or every N pages.
class SplitDialog : public QDialog {
    Q_OBJECT
public:
    explicit SplitDialog(int totalPages, QWidget* parent = nullptr);

    enum Mode { ByRanges, EveryN, EachPage };
    Mode    mode()       const;
    QString rangeText()  const;  // valid when mode() == ByRanges
    int     everyN()     const;  // valid when mode() == EveryN

private:
    int     m_totalPages;
    QWidget* m_rangesWidget;
    QWidget* m_everyNWidget;
    QLineEdit* m_ranges;
    QSpinBox*  m_n;
    QCheckBox* m_modeEachPage;
    QCheckBox* m_modeEveryN;
};

// ── AddTextDialog ─────────────────────────────────────────────────────────────
// Dialog for injecting text into a PDF page content stream.
// Pre-populated with auto-detected font name and size from nearby text.
class AddTextDialog : public QDialog {
    Q_OBJECT
public:
    // detectedPdfFont: e.g. "Helvetica-Bold", "Times-Roman"  (standard PDF name)
    // detectedSize: font size in pt detected near the click point (0 = use 12)
    explicit AddTextDialog(const QString& detectedPdfFont = "Helvetica",
                           double detectedSize = 12.0,
                           QWidget* parent = nullptr);

    QString text()        const;   // text the user typed
    QString pdfFontName() const;   // e.g. "Helvetica-Bold"
    double  fontSize()    const;
    QColor  color()       const;

private slots:
    void pickColor();
    void updateColorButton();

private:
    QPlainTextEdit*  m_textEdit;
    QComboBox*       m_familyCombo;   // Helvetica / Times / Courier
    QCheckBox*       m_boldCheck;
    QCheckBox*       m_italicCheck;
    QDoubleSpinBox*  m_sizeBox;
    QPushButton*     m_colorBtn;
    QColor           m_color;
};

// ── CropDialog ────────────────────────────────────────────────────────────────
// Specify crop margins (points) applied to the current page or all pages.
class CropDialog : public QDialog {
    Q_OBJECT
public:
    explicit CropDialog(QWidget* parent = nullptr);

    double top()    const;
    double bottom() const;
    double left()   const;
    double right()  const;
    bool   allPages() const;

private:
    QDoubleSpinBox* m_top;
    QDoubleSpinBox* m_bottom;
    QDoubleSpinBox* m_left;
    QDoubleSpinBox* m_right;
    QCheckBox*      m_allPages;
};
