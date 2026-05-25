#include "toc.h"
#include <poppler-qt6.h>

// Poppler 0.74+ provides outline() which replaces the removed toc() QDom API.
static void walkOutline(const QVector<Poppler::OutlineItem>& items, int level,
                        QVector<TocEntry>& out)
{
    for (const auto& item : items) {
        if (item.isNull()) continue;
        int pageIdx = 0;
        if (auto dest = item.destination())
            pageIdx = qMax(0, dest->pageNumber() - 1);
        out.append({ item.name(), pageIdx, level });
        if (item.hasChildren())
            walkOutline(item.children(), level + 1, out);
    }
}

QVector<TocEntry> readToc(Poppler::Document* doc) {
    QVector<TocEntry> entries;
    if (!doc) return entries;
    walkOutline(doc->outline(), 0, entries);
    return entries;
}
