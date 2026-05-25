#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include "config.h"

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Config* config, QWidget* parent = nullptr);

private slots:
    void onAccept();

private:
    Config*    m_config;
    QSpinBox*  m_maxRecent;
    QCheckBox* m_restoreLastDoc;
    QComboBox* m_defaultZoom;
    QCheckBox* m_showToc;
    QCheckBox* m_showPagePanel;
};
