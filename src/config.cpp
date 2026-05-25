#include "config.h"
#include <QDir>

Config::Config(QObject* parent)
    : QObject(parent), m_s("RincolTech", "PDFMaestro") {}

QStringList Config::coerceList(const QVariant& v) {
    if (v.isNull()) return {};
    if (v.typeId() == QMetaType::QString) {
        const auto s = v.toString();
        return s.isEmpty() ? QStringList{} : QStringList{s};
    }
    return v.toStringList();
}

void Config::addRecentFile(const QString& path) {
    auto files = coerceList(m_s.value("recent_files"));
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > maxRecentFiles()) files.removeLast();
    m_s.setValue("recent_files", files);
}

QStringList Config::recentFiles() const {
    return coerceList(m_s.value("recent_files"));
}

void Config::clearRecentFiles() { m_s.remove("recent_files"); }

int  Config::maxRecentFiles() const { return m_s.value("max_recent", 8).toInt(); }
void Config::setMaxRecentFiles(int n) { m_s.setValue("max_recent", n); }

bool Config::restoreLastDocument() const { return m_s.value("restore_last", false).toBool(); }
void Config::setRestoreLastDocument(bool v) { m_s.setValue("restore_last", v); }

QString Config::defaultZoom() const { return m_s.value("default_zoom", "100%").toString(); }
void    Config::setDefaultZoom(const QString& t) { m_s.setValue("default_zoom", t); }

bool Config::showTocPanel() const  { return m_s.value("show_toc", true).toBool(); }
void Config::setShowTocPanel(bool v) { m_s.setValue("show_toc", v); }

bool Config::showPagePanel() const  { return m_s.value("show_pages", true).toBool(); }
void Config::setShowPagePanel(bool v) { m_s.setValue("show_pages", v); }

QByteArray Config::windowGeometry() const { return m_s.value("win_geometry").toByteArray(); }
void       Config::setWindowGeometry(const QByteArray& d) { m_s.setValue("win_geometry", d); }

QByteArray Config::windowState() const { return m_s.value("win_state").toByteArray(); }
void       Config::setWindowState(const QByteArray& d) { m_s.setValue("win_state", d); }

QString Config::lastOpenDir() const { return m_s.value("last_open_dir", QDir::homePath()).toString(); }
void    Config::setLastOpenDir(const QString& p) { m_s.setValue("last_open_dir", p); }
