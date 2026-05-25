#pragma once
#include <QDialog>
#include <QImage>
#include <QTabWidget>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QSlider>

class DrawCanvas;

class SignatureDialog : public QDialog {
    Q_OBJECT
public:
    explicit SignatureDialog(QWidget* parent = nullptr);

    // Returns the signature as a transparent ARGB image, or null if cancelled.
    QImage result() const { return m_result; }

private slots:
    void onAccept();

private:
    QImage renderTyped() const;
    QImage renderDrawn() const;
    QImage renderUploaded() const;

    QTabWidget* m_tabs;

    // Typed tab
    QLineEdit*  m_typeInput;
    QComboBox*  m_fontCombo;
    QComboBox*  m_colorCombo;
    QLabel*     m_typePreview;

    // Draw tab
    DrawCanvas* m_canvas;
    QSlider*    m_brushSize;

    // Upload tab
    QLabel*     m_uploadPreview;
    QString     m_uploadPath;

    QImage      m_result;
};

// Freehand drawing canvas used by the Draw tab
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
