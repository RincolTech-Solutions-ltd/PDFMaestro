#pragma once
#include <QString>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <qpdf/QPDF.hh>

namespace Annotations {

struct Quad { double x0,y0,x1,y1,x2,y2,x3,y3; };

void addHighlight(QPDF& pdf, int pageIdx,
                  const QVector<Quad>& quads,
                  QColor color = QColor(255,230,0),
                  double opacity = 0.4);

void addTextNote(QPDF& pdf, int pageIdx,
                 double x, double y,
                 const QString& contents,
                 QColor color = QColor(255,255,0));

void addInk(QPDF& pdf, int pageIdx,
            const QVector<QVector<QPointF>>& strokes,
            QColor color = QColor(0,0,200),
            double width = 2.0);

void addStamp(QPDF& pdf, int pageIdx,
              double x, double y,
              double w, double h,
              const QString& name);

void addRedact(QPDF& pdf, int pageIdx,
               double x0, double y0, double x1, double y1);

void applyRedactions(QPDF& pdf, int pageIdx);

} // namespace Annotations
