#pragma once
#include <QObject>
#include <QStringList>
#include <QByteArray>
#include <QSettings>

class Config : public QObject {
    Q_OBJECT
public:
    explicit Config(QObject* parent = nullptr);

    // Recent files
    void        addRecentFile(const QString& path);
    QStringList recentFiles() const;
    void        clearRecentFiles();
    int         maxRecentFiles() const;
    void        setMaxRecentFiles(int n);

    // Restore on launch
    bool restoreLastDocument() const;
    void setRestoreLastDocument(bool v);

    // View
    QString defaultZoom() const;
    void    setDefaultZoom(const QString& text);
    bool    showTocPanel() const;
    void    setShowTocPanel(bool v);
    bool    showPagePanel() const;
    void    setShowPagePanel(bool v);

    // Window state
    QByteArray windowGeometry() const;
    void       setWindowGeometry(const QByteArray& data);
    QByteArray windowState() const;
    void       setWindowState(const QByteArray& data);

    // Last open directory
    QString lastOpenDir() const;
    void    setLastOpenDir(const QString& path);

private:
    QSettings m_s;
    static QStringList coerceList(const QVariant& v);
};
