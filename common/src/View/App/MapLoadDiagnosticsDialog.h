#pragma once

#include "Logger.h"
#include <QDialog>
#include <filesystem>

namespace TrenchBroom::View {
class MapLoadDiagnosticsDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(int initialWidth MEMBER m_initialWidth)
    Q_PROPERTY(int initialHeight MEMBER m_initialHeight)
    int m_initialWidth = 0, m_initialHeight = 0;
public:
    MapLoadDiagnosticsDialog(const std::filesystem::path& path, const std::vector<LogMessage>& messages, QWidget* parent = nullptr);
};
void showMapLoadDiagnostics(const std::filesystem::path& path, const std::vector<LogMessage>& messages, QWidget* parent);
}
