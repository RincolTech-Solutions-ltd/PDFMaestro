#pragma once
#include <QString>
#include <QVector>
#include <QPair>
#include <QImage>
#include <QBuffer>
#include <QColor>
#include <qpdf/QPDF.hh>

namespace Operations {

struct Range { int first; int last; }; // 0-based inclusive

// Page order: indices[i] = original index that should go to position i
void applyPageOrder(QPDF& pdf, const QVector<int>& order);
void deletePages(QPDF& pdf, const QVector<int>& indices);
void rotatePage(QPDF& pdf, int pageIdx, int degrees);
void cropPage(QPDF& pdf, int pageIdx,
              double left, double bottom, double right, double top);
void cropAllPages(QPDF& pdf,
                  double left, double bottom, double right, double top);

// Merge other PDFs into pdf (appends pages at insertAt, -1 = end)
void mergeInto(QPDF& pdf, const QStringList& otherPaths, int insertAt = -1);
void mergeInto(QPDF& pdf, QPDF& src, int insertAt = -1);

// Split: returns list of output paths written
QStringList splitByRanges(QPDF& pdf, const QVector<Range>& ranges,
                           const QString& outDir, const QString& baseName);
QStringList splitEveryN(QPDF& pdf, int n,
                        const QString& outDir, const QString& baseName);
QStringList splitEachPage(QPDF& pdf,
                          const QString& outDir, const QString& baseName);

// Parse "1-3,5,7-9" into 0-based Range vector
QVector<Range> parseRanges(const QString& text, int total);

// Overlay a signature PDF on a page
void overlaySignatureOnPage(QPDF& pdf, int pageIdx,
                             const QString& sigPdfPath,
                             const QString& position = "bottom-right",
                             double margin = 20.0);

// Overlay a QImage at explicit PDF coordinates (x,y = bottom-left, in PDF points)
void overlayImageOnPage(QPDF& pdf, int pageIdx, const QImage& img,
                        double sigW, double sigH, double x, double y);
// Convenience: place at bottom-right with margin
void overlayImageOnPage(QPDF& pdf, int pageIdx, const QImage& img,
                        double sigW, double sigH, double margin = 20.0);

// Inject plain text directly into the page content stream.
// (x, y) = baseline start in PDF points, origin bottom-left.
// pdfFontName must be one of the 14 standard PDF Type1 font names
// (e.g. "Helvetica", "Helvetica-Bold", "Times-Roman", "Courier").
// Multi-line text: split by '\n', lines spaced by fontSize.
void injectText(QPDF& pdf, int pageIdx,
                double x, double y,
                const QString& text,
                const QString& pdfFontName,
                double fontSize,
                QColor color = Qt::black);

// Serialize pdf to QByteArray (for in-memory Poppler reload)
QByteArray toBytes(QPDF& pdf);

} // namespace Operations
