#include "AssistantPanel.h"

#include "../application/Application.h"
#include "../application/Settings.h"

#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace eda {

AssistantPanel::AssistantPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_details = new QTextEdit(this);
    m_details->setReadOnly(true);
    layout->addWidget(m_details, 1);

    auto *configureButton = new QPushButton(tr("Configure AI provider..."), this);
    connect(configureButton, &QPushButton::clicked, this, &AssistantPanel::configureRequested);
    layout->addWidget(configureButton);

    m_privacyLabel = new QLabel(this);
    m_privacyLabel->setWordWrap(true);
    m_privacyLabel->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    layout->addWidget(m_privacyLabel);

    refreshFromSettings();
}

QString AssistantPanel::statusSummary() const { return m_statusLabel->text(); }

void AssistantPanel::refreshFromSettings()
{
    Settings *settings = Application::instance() ? Application::instance()->settings() : nullptr;
    const QString provider = settings
        ? settings->value(QStringLiteral("ai/provider"), QStringLiteral("none")).toString()
        : QStringLiteral("none");
    const QString model = settings ? settings->value(QStringLiteral("ai/model")).toString() : QString();

    if (provider.isEmpty() || provider == QLatin1String("none")) {
        m_statusLabel->setText(tr("AI: not configured"));
        m_details->setPlainText(
            tr("No AI provider is configured.\n\n"
               "The application works without one: board viewing, component navigation, "
               "test points, measurements, the knowledge database and every deterministic "
               "diagnostic function are available offline.\n\n"
               "Configure a local provider (for example an Ollama server) or an optional "
               "cloud provider in Settings to enable the diagnostic assistant."));
    } else {
        m_statusLabel->setText(tr("AI: %1 (%2)").arg(provider, model.isEmpty() ? tr("no model") : model));
        m_details->setPlainText(
            tr("The provider is configured. The diagnostic assistant is not connected yet "
               "in this build."));
    }

    const bool networkAllowed =
        settings ? settings->value(QStringLiteral("privacy/allowNetwork"), false).toBool() : false;
    m_privacyLabel->setText(networkAllowed
                                ? tr("Network access is allowed by your settings.")
                                : tr("Network access is disabled. Only local providers can be used."));
}

} // namespace eda
