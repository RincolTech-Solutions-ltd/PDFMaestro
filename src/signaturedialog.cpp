#include "signaturedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QFontDatabase>
#include <QMessageBox>

// ── DrawCanvas ────────────────────────────────────────────────────────────────

DrawCanvas::DrawCanvas(QWidget* parent) : QWidget(parent) {
    setFixedSize(400, 150);
    m_canvas = QImage(size(), QImage::Format_ARGB32);
    m_canvas.fill(Qt::transparent);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
}

void DrawCanvas::clear() {
    m_canvas.fill(Qt::transparent);
    update();
}

QImage DrawCanvas::toImage() const { return m_canvas; }

void DrawCanvas::mousePressEvent(QMouseEvent* e) {
    m_last    = e->position();
    m_drawing = true;
}

void DrawCanvas::mouseMoveEvent(QMouseEvent* e) {
    if (!m_drawing) return;
    QPainter p(&m_canvas);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(m_color, m_brushPx, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(m_last, e->position());
    m_last = e->position();
    update();
}

void DrawCanvas::mouseReleaseEvent(QMouseEvent*) { m_drawing = false; }

void DrawCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(245, 245, 245));
    p.drawImage(0, 0, m_canvas);
    p.setPen(QPen(QColor(180, 180, 180), 1, Qt::DashLine));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ── SignatureDialog ───────────────────────────────────────────────────────────

static const QStringList COLOR_NAMES = { "Black", "Blue", "Dark Green" };
static const QList<QColor> COLORS    = { Qt::black, QColor(0,0,180), QColor(0,100,0) };

SignatureDialog::SignatureDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Insert Signature");
    setMinimumWidth(480);

    auto* root = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    // ── Typed tab ──────────────────────────────────────────────────────────
    auto* typedWidget = new QWidget;
    auto* typedForm   = new QFormLayout(typedWidget);

    m_typeInput = new QLineEdit;
    m_typeInput->setPlaceholderText("Type your name");
    typedForm->addRow("Name:", m_typeInput);

    m_fontCombo = new QComboBox;
    QStringList scriptFonts;
    for (const auto& f : QFontDatabase::families())
        if (QFontDatabase::isScalable(f))
            scriptFonts << f;
    m_fontCombo->addItems(scriptFonts.mid(0, qMin(scriptFonts.size(), 60)));
    typedForm->addRow("Font:", m_fontCombo);

    m_colorCombo = new QComboBox;
    m_colorCombo->addItems(COLOR_NAMES);
    typedForm->addRow("Color:", m_colorCombo);

    m_typePreview = new QLabel;
    m_typePreview->setFixedHeight(60);
    m_typePreview->setAlignment(Qt::AlignCenter);
    m_typePreview->setStyleSheet("background:#f5f5f5; border:1px dashed #aaa;");
    typedForm->addRow("Preview:", m_typePreview);

    connect(m_typeInput,  &QLineEdit::textChanged, this, [this](){ m_typePreview->setPixmap(QPixmap::fromImage(renderTyped())); });
    connect(m_fontCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](){ m_typePreview->setPixmap(QPixmap::fromImage(renderTyped())); });
    connect(m_colorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](){ m_typePreview->setPixmap(QPixmap::fromImage(renderTyped())); });

    m_tabs->addTab(typedWidget, "Type");

    // ── Draw tab ───────────────────────────────────────────────────────────
    auto* drawWidget = new QWidget;
    auto* drawLayout = new QVBoxLayout(drawWidget);

    auto* drawTools = new QHBoxLayout;
    auto* clearBtn  = new QPushButton("Clear");
    auto* colorDraw = new QComboBox;
    colorDraw->addItems(COLOR_NAMES);

    m_brushSize = new QSlider(Qt::Horizontal);
    m_brushSize->setRange(1, 8);
    m_brushSize->setValue(3);

    drawTools->addWidget(new QLabel("Color:"));
    drawTools->addWidget(colorDraw);
    drawTools->addWidget(new QLabel("Size:"));
    drawTools->addWidget(m_brushSize);
    drawTools->addStretch();
    drawTools->addWidget(clearBtn);

    m_canvas = new DrawCanvas;
    drawLayout->addLayout(drawTools);
    drawLayout->addWidget(m_canvas, 0, Qt::AlignHCenter);

    connect(clearBtn, &QPushButton::clicked, m_canvas, &DrawCanvas::clear);
    connect(colorDraw, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int i){ m_canvas->setBrushColor(COLORS.value(i, Qt::black)); });
    connect(m_brushSize, &QSlider::valueChanged, this,
            [this](int v){ m_canvas->setBrushSize(v); });

    m_tabs->addTab(drawWidget, "Draw");

    // ── Upload tab ─────────────────────────────────────────────────────────
    auto* uploadWidget = new QWidget;
    auto* uploadLayout = new QVBoxLayout(uploadWidget);

    auto* browseBtn = new QPushButton("Browse Image…");
    m_uploadPreview = new QLabel("No image selected");
    m_uploadPreview->setFixedHeight(120);
    m_uploadPreview->setAlignment(Qt::AlignCenter);
    m_uploadPreview->setStyleSheet("background:#f5f5f5; border:1px dashed #aaa;");

    connect(browseBtn, &QPushButton::clicked, this, [this](){
        m_uploadPath = QFileDialog::getOpenFileName(this, "Select Signature Image",
            {}, "Images (*.png *.jpg *.jpeg *.bmp *.svg)");
        if (!m_uploadPath.isEmpty()) {
            QPixmap pm(m_uploadPath);
            if (!pm.isNull())
                m_uploadPreview->setPixmap(pm.scaled(
                    m_uploadPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    });

    uploadLayout->addWidget(browseBtn);
    uploadLayout->addWidget(m_uploadPreview);
    uploadLayout->addStretch();
    m_tabs->addTab(uploadWidget, "Upload");

    // ── Buttons ────────────────────────────────────────────────────────────
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &SignatureDialog::onAccept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(m_tabs);
    root->addWidget(btns);
}

void SignatureDialog::onAccept() {
    QImage img;
    int tab = m_tabs->currentIndex();
    if (tab == 0) img = renderTyped();
    else if (tab == 1) img = renderDrawn();
    else img = renderUploaded();

    if (img.isNull()) {
        QMessageBox::warning(this, "Signature", "No signature to insert.");
        return;
    }
    m_result = img;
    accept();
}

QImage SignatureDialog::renderTyped() const {
    const QString text = m_typeInput->text().trimmed();
    if (text.isEmpty()) return {};

    QFont font(m_fontCombo->currentText(), 36);
    font.setItalic(true);
    QFontMetrics fm(font);
    QRect br = fm.boundingRect(text);
    QImage img(br.width() + 20, br.height() + 10, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setFont(font);
    p.setPen(COLORS.value(m_colorCombo->currentIndex(), Qt::black));
    p.drawText(img.rect(), Qt::AlignCenter, text);
    return img;
}

QImage SignatureDialog::renderDrawn() const {
    return m_canvas->toImage();
}

QImage SignatureDialog::renderUploaded() const {
    if (m_uploadPath.isEmpty()) return {};
    QImage img(m_uploadPath);
    return img.convertToFormat(QImage::Format_ARGB32);
}
