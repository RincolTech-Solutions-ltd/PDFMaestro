#pragma once
#include <QDialog>
#include <QImage>
#include <QTabWidget>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QButtonGroup>

class DrawCanvas;

class SignatureDialog : public QDialog {
    Q_OBJECT
public:
    explicit SignatureDialog(QWidget* parent = nullptr);

    // Returns the signature as a transparent ARGB image, or null if cancelled.
    QImage result() const { return m_result; }

private slots:
    void onAccept();
    void onFileSelected();

private:
    QImage renderTyped() const;
    QImage renderDrawn() const;
    QImage renderUploaded() const;

    // Ports of pdfarranger image_utils.py — exact same algorithm
    static QImage removeBg(QImage img, int threshold = 200);
    static QImage autocropAlpha(QImage img, int margin = 8);
    static QImage boostContrast(QImage img, double factor = 3.0);

    void refreshVariantThumbnails(const QImage& orig);

    QTabWidget* m_tabs;

    // Typed tab
    QLineEdit*  m_typeInput;
    QComboBox*  m_fontCombo;
    QComboBox*  m_colorCombo;
    QLabel*     m_typePreview;

    // Draw tab
    DrawCanvas* m_canvas;
    QSlider*    m_brushSize;

    // Upload tab — 3 variants exactly like pdfarranger
    // [0] = Original   [1] = Transparent A (remove_bg 200)   [2] = Transparent B (boost+remove_bg 160)
    QLabel*       m_varImg[3];
    QButtonGroup* m_varGroup = nullptr;
    QImage        m_varImages[3];
    int           m_selVariant = 1;   // default: Transparent A

    QImage m_result;
};

// ── Freehand drawing canvas ────────────────────────────────────────────────────
class DrawCanvas : public QWidget {
    Q_OBJECT
public:
    explicit DrawCanvas(QWidget* parent = nullptr);
    QImage toImage() const;
    void   clear();
    void   setBrushSize(int px) { m_brushPx = px; }
    void   setBrushColor(const QColor& c) { m_color = c; }

protected:
    void mousePressEvent(QMouseEvent* e)   override;
    void mouseMoveEvent(QMouseEvent* e)    override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e)        override;

private:
    QImage  m_canvas;
    QPointF m_last;
    bool    m_drawing = false;
    int     m_brushPx = 3;
    QColor  m_color   = Qt::black;
};
