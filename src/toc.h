#pragma once
#include <QString>
#include <QVector>

namespace Poppler { class Document; }

struct TocEntry {
    QString title;
    int     pageIndex = -1;
    int     level     = 0;
};

QVector<TocEntry> readToc(Poppler::Document* doc);
