#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

class ConsoleWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConsoleWidget(QWidget* parent = nullptr) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // Header bar
        auto* bar = new QWidget;
        bar->setObjectName("consoleBar");
        bar->setFixedHeight(32);
        bar->setStyleSheet("background: #16213e; border-top: 1px solid #2d4a7a;");
        auto* barLayout = new QHBoxLayout(bar);
        barLayout->setContentsMargins(12, 0, 8, 0);
        barLayout->setSpacing(8);

        auto* title = new QLabel("Console Output");
        title->setStyleSheet("color: #90caf9; font-size: 11px; font-weight: 600;");
        barLayout->addWidget(title);
        barLayout->addStretch();

        auto* saveBtn = new QPushButton("Save Log");
        saveBtn->setFixedHeight(22);
        saveBtn->setStyleSheet("font-size: 11px; padding: 0 8px;");
        connect(saveBtn, &QPushButton::clicked, this, &ConsoleWidget::saveLog);
        barLayout->addWidget(saveBtn);

        auto* clearBtn = new QPushButton("Clear");
        clearBtn->setFixedHeight(22);
        clearBtn->setStyleSheet("font-size: 11px; padding: 0 8px;");
        connect(clearBtn, &QPushButton::clicked, this, &ConsoleWidget::clear);
        barLayout->addWidget(clearBtn);

        layout->addWidget(bar);

        m_edit = new QPlainTextEdit;
        m_edit->setReadOnly(true);
        m_edit->setMaximumBlockCount(5000);
        m_edit->setStyleSheet(
            "background: #0a0f1a; color: #a0c8a0; font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 12px;"
            "border: none; border-top: 1px solid #1a2a3a;");
        layout->addWidget(m_edit, 1);
    }

    void appendLine(const QString& line) {
        // Colorize common patterns
        QString colored = line.toHtmlEscaped();
        if (line.contains("[ERROR]") || line.contains("Exception") || line.contains("Error:"))
            colored = "<span style='color:#ff6b6b'>" + colored + "</span>";
        else if (line.contains("[WARN]") || line.contains("WARNING"))
            colored = "<span style='color:#ffd93d'>" + colored + "</span>";
        else if (line.contains("[INFO]") || line.startsWith("Starting"))
            colored = "<span style='color:#74c7ec'>" + colored + "</span>";
        else if (line.startsWith("[STDERR]"))
            colored = "<span style='color:#f38ba8'>" + colored + "</span>";

        m_edit->appendHtml(colored);
        auto* sb = m_edit->verticalScrollBar();
        if (sb) sb->setValue(sb->maximum());
    }

    void clear() { m_edit->clear(); }

    void saveLog() {
        QString path = QFileDialog::getSaveFileName(this, "Save Log", "klauncher.log", "Log Files (*.log *.txt)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << m_edit->toPlainText();
        }
    }

private:
    QPlainTextEdit* m_edit = nullptr;
};
