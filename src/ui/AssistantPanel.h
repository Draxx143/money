// Right dock: AI diagnostic assistant.
//
// Before a provider is configured the panel says so and offers a way to
// configure one; it never pretends that a model is available. The chat and the
// structured diagnostic workflow are added when the provider layer exists -
// until then every control that could not do anything real is simply absent.
#pragma once

#include <QWidget>

class QLabel;
class QTextEdit;

namespace eda {

class AssistantPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AssistantPanel(QWidget *parent = nullptr);

    // Text shown in the status bar next to the panel.
    QString statusSummary() const;

public slots:
    void refreshFromSettings();

signals:
    void configureRequested();

private:
    QLabel *m_statusLabel = nullptr;
    QTextEdit *m_details = nullptr;
    QLabel *m_privacyLabel = nullptr;
};

} // namespace eda
