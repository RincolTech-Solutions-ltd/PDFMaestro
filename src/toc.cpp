#include "toc.h"
#include <poppler-qt6.h>

static void walk(QDomNode node, int level, QVector<TocEntry>& out, Poppler::Document* doc) {
    while (!node.isNull()) {
        QDomElement el = node.toElement();
        if (!el.isNull()) {
            TocEntry entry;
            entry.title = el.tagName();
            entry.level = level;

            // Resolve page number from destination
            QString dest = el.attribute("Destination");
            QString destName = el.attribute("DestinationName");
            if (!dest.isEmpty()) {
                auto linkDest = doc->linkDestination(dest);
                if (linkDest) entry.pageIndex = linkDest->pageNumber() - 1;
            } else if (!destName.isEmpty()) {
                auto linkDest = doc->linkDestination(destName);
                if (linkDest) entry.pageIndex = linkDest->pageNumber() - 1;
            }

            out.append(entry);

            if (el.hasChildNodes())
                walk(el.firstChild(), level + 1, out, doc);
        }
        node = node.nextSibling();
    }
}

QVector<TocEntry> readToc(Poppler::Document* doc) {
    QVector<TocEntry> entries;
    if (!doc) return entries;

    QDomDocument* toc = doc->toc();
    if (!toc) return entries;

    walk(toc->firstChild(), 0, entries, doc);
    delete toc;
    return entries;
}
