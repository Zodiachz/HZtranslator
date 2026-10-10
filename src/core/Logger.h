#pragma once

#include <QString>

namespace hypers {

// File + debugger logging installed as the Qt message handler.
// Log file: %LOCALAPPDATA%\Hypers\HYPERS TRANSLATOR\logs\hypers.log (rotated at 2 MB).
// Translated content and API keys are never logged - only lengths and metadata.
class Logger {
public:
    static void install();
    static QString logFilePath();
};

} // namespace hypers
