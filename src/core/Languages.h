#pragma once

#include <QList>
#include <QString>

namespace hypers {

// One row of the language table. `code` is the app-internal identifier that is
// persisted in settings; the provider-specific columns map it to the codes each
// API expects. An empty provider column means "not supported by that provider".
struct LanguageInfo {
    QString code;          // e.g. "en-US", "ar", "zh-Hans"
    QString englishName;   // e.g. "English (US)"
    QString nativeName;    // e.g. "English"
    QString deeplSource;   // DeepL source_lang, e.g. "EN"
    QString deeplTarget;   // DeepL target_lang, e.g. "EN-US"
    QString google;        // Google Cloud Translation v2 code, e.g. "en"
    bool rightToLeft = false;
};

class Languages {
public:
    static constexpr const char* kAuto = "auto";

    static const QList<LanguageInfo>& all();
    static const LanguageInfo* find(const QString& code);

    // "English (US) - English" style label for combo boxes.
    static QString label(const QString& code);

    // Maps a language code reported by a provider (DeepL "EN", Google "iw", ...)
    // to a human readable name without the regional suffix, e.g. "English".
    static QString nameForProviderCode(const QString& providerCode);

    static bool isRightToLeft(const QString& code);
};

} // namespace hypers
