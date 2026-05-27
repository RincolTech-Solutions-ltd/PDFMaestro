#include "signaturedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QFontDatabase>
#include <QMessageBox>
#include <QGroupBox>
#include <algorithm>
#include <cmath>

// ── DrawCanvas ─────────────────────────────────────────────────────────────────

DrawCanvas::DrawCanvas(QWidget* parent) : QWidget(parent) {
    setFixedSize(460, 160);
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
    // Checkerboard background so transparency is visible
    const int cs = 10;
    for (int y = 0; y < height(); y += cs)
        for (int x = 0; x < width(); x += cs)
            p.fillRect(x, y, cs, cs, ((x / cs + y / cs) % 2 == 0)
                       ? QColor(220, 220, 220) : QColor(255, 255, 255));
    p.drawImage(0, 0, m_canvas);
    p.setPen(QPen(QColor(180, 180, 180), 1, Qt::SolidLine));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ── image_utils.py ports ───────────────────────────────────────────────────────

// Port of image_utils.remove_bg — luminosity threshold → alpha, smooth edges
QImage SignatureDialog::removeBg(QImage img, int threshold) {
    img = img.convertToFormat(QImage::Format_ARGB32);
    const int cutoff = 255 - threshold;
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb  pix = line[x];
            int r = qRed(pix), g = qGreen(pix), b = qBlue(pix);
            // Luminosity (ITU-R BT.601 — same as PIL's L-mode)
            int lum      = (r * 299 + g * 587 + b * 114) / 1000;
            int inverted = 255 - lum;
            int alpha;
            if (inverted <= cutoff) {
                alpha = 0;
            } else {
                alpha = std::min(255, (inverted - cutoff) * 255
                                      / std::max(1, 255 - cutoff));
            }
            line[x] = qRgba(r, g, b, alpha);
        }
    }
    return img;
}

// Port of image_utils.autocrop_alpha — tight crop to non-transparent pixels + margin
QImage SignatureDialog::autocropAlpha(QImage img, int margin) {
    img = img.convertToFormat(QImage::Format_ARGB32);
    int minX = img.width(), maxX = -1, minY = img.height(), maxY = -1;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) > 0) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX < 0) return img;  // fully transparent — nothing to crop
    int left   = std::max(0, minX - margin);
    int top    = std::max(0, minY - margin);
    int right  = std::min(img.width()  - 1, maxX + margin);
    int bottom = std::min(img.height() - 1, maxY + margin);
    return img.copy(left, top, right - left + 1, bottom - top + 1);
}

// Port of image_utils.boost_contrast — stretch RGB values around midpoint
QImage SignatureDialog::boostContrast(QImage img, double factor) {
    img = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb pix = line[x];
            int  a   = qAlpha(pix);
            auto enhance = [factor](int v) {
                return qBound(0, (int)std::round((v - 128) * factor + 128), 255);
            };
            line[x] = qRgba(enhance(qRed(pix)),
                             enhance(qGreen(pix)),
                             enhance(qBlue(pix)), a);
        }
    }
    return img;
}

// ── SignatureDialog ────────────────────────────────────────────────────────────

static const QStringList COLOR_NAMES = { "Black", "Blue", "Dark Green" };
static const QList<QColor> COLORS    = { Qt::black, QColor(0,0,180), QColor(0,100,0) };

SignatureDialog::SignatureDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Insert Signature");
    setMinimumWidth(520);

    auto* root = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    // ── Typed tab ──────────────────────────────────────────────────────────────
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

    auto refreshPreview = [this](){
        m_typePreview->setPixmap(QPixmap::fromImage(renderTyped()));
    };
    connect(m_typeInput,  &QLineEdit::textChanged,
            this, [refreshPreview](){ refreshPreview(); });
    connect(m_fontCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [refreshPreview](int){ refreshPreview(); });
    connect(m_colorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [refreshPreview](int){ refreshPreview(); });

    m_tabs->addTab(typedWidget, "⌨  Type");

    // ── Draw tab ───────────────────────────────────────────────────────────────
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

    connect(clearBtn,   &QPushButton::clicked, m_canvas, &DrawCanvas::clear);
    connect(colorDraw,  QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int i){ m_canvas->setBrushColor(COLORS.value(i, Qt::black)); });
    connect(m_brushSize, &QSlider::valueChanged, this,
            [this](int v){ m_canvas->setBrushSize(v); });

    m_tabs->addTab(drawWidget, "✍  Draw");

    // ── Upload tab — 3 variants like pdfarranger ───────────────────────────────
    auto* uploadWidget = new QWidget;
    auto* uploadLayout = new QVBoxLayout(uploadWidget);

    auto* browseRow = new QHBoxLayout;
    auto* browseBtn = new QPushButton("Browse Image…");
    browseRow->addWidget(browseBtn);
    browseRow->addStretch();
    uploadLayout->addLayout(browseRow);

    uploadLayout->addWidget(new QLabel("Choose an image version:"));

    // Three thumbnail columns: Original | Transparent A | Transparent B
    m_varGroup = new QButtonGroup(this);
    auto* thumbsLayout = new QHBoxLayout;
    thumbsLayout->setSpacing(16);

    static const char* captions[] = { "Original", "Transparent A", "Transparent B" };
    for (int i = 0; i < 3; ++i) {
        auto* col = new QVBoxLayout;
        col->setAlignment(Qt::AlignHCenter);

        m_varImg[i] = new QLabel;
        m_varImg[i]->setFixedSize(160, 110);
        m_varImg[i]->setAlignment(Qt::AlignCenter);
        // Checkerboard background so transparency is visible in thumbnails
        m_varImg[i]->setStyleSheet(
            "QLabel { background-image: url(\":/icons/checkerboard\"); "
            "border: 2px solid #ccc; border-radius: 4px; }");

        auto* rb = new QRadioButton(captions[i]);
        rb->setChecked(i == m_selVariant);
        m_varGroup->addButton(rb, i);

        col->addWidget(m_varImg[i], 0, Qt::AlignHCenter);
        col->addWidget(rb, 0, Qt::AlignHCenter);
        thumbsLayout->addLayout(col);
    }
    uploadLayout->addLayout(thumbsLayout);
    uploadLayout->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, &SignatureDialog::onFileSelected);
    connect(m_varGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id){ m_selVariant = id; });

    m_tabs->addTab(uploadWidget, "📄  Upload");

    // ── Buttons ────────────────────────────────────────────────────────────────
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &SignatureDialog::onAccept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(m_tabs);
    root->addWidget(btns);
}

// ── Slots ──────────────────────────────────────────────────────────────────────

void SignatureDialog::onFileSelected() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Signature Image", {},
        "Images (*.png *.jpg *.jpeg *.bmp)");
    if (path.isEmpty()) return;

    QImage orig(path);
    if (orig.isNull()) return;
    orig = orig.convertToFormat(QImage::Format_ARGB32);
    refreshVariantThumbnails(orig);
}

void SignatureDialog::refreshVariantThumbnails(const QImage& orig) {
    // Exact port of pdfarranger signature.py _on_file_set():
    //   trans_a = autocrop_alpha(remove_bg(orig, threshold=200))
    //   trans_b = autocrop_alpha(remove_bg(boost_contrast(orig), threshold=160))
    m_varImages[0] = orig;
    m_varImages[1] = autocropAlpha(removeBg(orig, 200));
    m_varImages[2] = autocropAlpha(removeBg(boostContrast(orig, 3.0), 160));

    const QSize thumbSize(156, 106);
    for (int i = 0; i < 3; ++i) {
        if (m_varImages[i].isNull()) continue;
        QPixmap pm = QPixmap::fromImage(m_varImages[i])
                         .scaled(thumbSize, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
        m_varImg[i]->setPixmap(pm);
    }
}

void SignatureDialog::onAccept() {
    QImage img;
    int tab = m_tabs->currentIndex();
    if (tab == 0)      img = renderTyped();
    else if (tab == 1) img = renderDrawn();
    else               img = renderUploaded();

    if (img.isNull()) {
        QMessageBox::warning(this, "Signature", "No signature to insert.");
        return;
    }
    m_result = img;
    accept();
}

// ── Render helpers ─────────────────────────────────────────────────────────────

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
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(font);
    p.setPen(COLORS.value(m_colorCombo->currentIndex(), Qt::black));
    p.drawText(img.rect(), Qt::AlignCenter, text);
    return img;
}

QImage SignatureDialog::renderDrawn() const {
    // autocrop so the result isn't a giant transparent canvas
    return autocropAlpha(m_canvas->toImage());
}

QImage SignatureDialog::renderUploaded() const {
    return m_varImages[m_selVariant];
}
