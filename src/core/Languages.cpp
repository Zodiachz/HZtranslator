#include "core/Languages.h"

namespace hypers {

namespace {

QList<LanguageInfo> buildTable()
{
    // clang-format off
    return {
        {"ar",      "Arabic",                  "العربية",            "AR", "AR",      "ar",    true },
        {"bn",      "Bengali",                 "বাংলা",              "",   "",        "bn",    false},
        {"bg",      "Bulgarian",               "Български",          "BG", "BG",      "bg",    false},
        {"zh-Hans", "Chinese (Simplified)",    "简体中文",            "ZH", "ZH-HANS", "zh-CN", false},
        {"zh-Hant", "Chinese (Traditional)",   "繁體中文",            "ZH", "ZH-HANT", "zh-TW", false},
        {"cs",      "Czech",                   "Čeština",            "CS", "CS",      "cs",    false},
        {"da",      "Danish",                  "Dansk",              "DA", "DA",      "da",    false},
        {"nl",      "Dutch",                   "Nederlands",         "NL", "NL",      "nl",    false},
        {"en-US",   "English (US)",            "English",            "EN", "EN-US",   "en",    false},
        {"en-GB",   "English (UK)",            "English",            "EN", "EN-GB",   "en",    false},
        {"et",      "Estonian",                "Eesti",              "ET", "ET",      "et",    false},
        {"fi",      "Finnish",                 "Suomi",              "FI", "FI",      "fi",    false},
        {"fr",      "French",                  "Français",           "FR", "FR",      "fr",    false},
        {"de",      "German",                  "Deutsch",            "DE", "DE",      "de",    false},
        {"el",      "Greek",                   "Ελληνικά",           "EL", "EL",      "el",    false},
        {"he",      "Hebrew",                  "עברית",              "",   "",        "he",    true },
        {"hi",      "Hindi",                   "हिन्दी",              "",   "",        "hi",    false},
        {"hu",      "Hungarian",               "Magyar",             "HU", "HU",      "hu",    false},
        {"id",      "Indonesian",              "Bahasa Indonesia",   "ID", "ID",      "id",    false},
        {"it",      "Italian",                 "Italiano",           "IT", "IT",      "it",    false},
        {"ja",      "Japanese",                "日本語",              "JA", "JA",      "ja",    false},
        {"ko",      "Korean",                  "한국어",              "KO", "KO",      "ko",    false},
        {"lv",      "Latvian",                 "Latviešu",           "LV", "LV",      "lv",    false},
        {"lt",      "Lithuanian",              "Lietuvių",           "LT", "LT",      "lt",    false},
        {"ms",      "Malay",                   "Bahasa Melayu",      "",   "",        "ms",    false},
        {"nb",      "Norwegian (Bokmål)",      "Norsk bokmål",       "NB", "NB",      "no",    false},
        {"fa",      "Persian",                 "فارسی",              "",   "",        "fa",    true },
        {"pl",      "Polish",                  "Polski",             "PL", "PL",      "pl",    false},
        {"pt-BR",   "Portuguese (Brazil)",     "Português (Brasil)", "PT", "PT-BR",   "pt",    false},
        {"pt-PT",   "Portuguese (Portugal)",   "Português",          "PT", "PT-PT",   "pt-PT", false},
        {"ro",      "Romanian",                "Română",             "RO", "RO",      "ro",    false},
        {"ru",      "Russian",                 "Русский",            "RU", "RU",      "ru",    false},
        {"sk",      "Slovak",                  "Slovenčina",         "SK", "SK",      "sk",    false},
        {"sl",      "Slovenian",               "Slovenščina",        "SL", "SL",      "sl",    false},
        {"es",      "Spanish",                 "Español",            "ES", "ES",      "es",    false},
        {"sv",      "Swedish",                 "Svenska",            "SV", "SV",      "sv",    false},
        {"th",      "Thai",                    "ไทย",                "",   "",        "th",    false},
        {"tr",      "Turkish",                 "Türkçe",             "TR", "TR",      "tr",    false},
        {"uk",      "Ukrainian",               "Українська",         "UK", "UK",      "uk",    false},
        {"ur",      "Urdu",                    "اردو",               "",   "",        "ur",    true },
        {"vi",      "Vietnamese",              "Tiếng Việt",         "",   "",        "vi",    false},
    };
    // clang-format on
}

QString stripRegion(const QString& name)
{
    const qsizetype paren = name.indexOf(QLatin1String(" ("));
    return paren > 0 ? name.left(paren) : name;
}

} // namespace

const QList<LanguageInfo>& Languages::all()
{
    static const QList<LanguageInfo> table = buildTable();
    return table;
}

const LanguageInfo* Languages::find(const QString& code)
{
    for (const LanguageInfo& lang : all()) {
        if (lang.code.compare(code, Qt::CaseInsensitive) == 0)
            return &lang;
    }
    return nullptr;
}

QString Languages::label(const QString& code)
{
    if (code == QLatin1String(kAuto))
        return QStringLiteral("Detect language");
    const LanguageInfo* lang = find(code);
    if (!lang)
        return code;
    if (stripRegion(lang->englishName) == lang->nativeName || lang->englishName == lang->nativeName)
        return lang->englishName;
    return lang->englishName + QStringLiteral("  ·  ") + lang->nativeName;
}

QString Languages::nameForProviderCode(const QString& providerCode)
{
    if (providerCode.isEmpty())
        return {};
    QString normalized = providerCode.trimmed();
    if (normalized.compare(QLatin1String("iw"), Qt::CaseInsensitive) == 0)
        normalized = QStringLiteral("he"); // legacy Google code for Hebrew

    for (const LanguageInfo& lang : all()) {
        if (lang.deeplSource.compare(normalized, Qt::CaseInsensitive) == 0
            || lang.deeplTarget.compare(normalized, Qt::CaseInsensitive) == 0
            || lang.google.compare(normalized, Qt::CaseInsensitive) == 0
            || lang.code.compare(normalized, Qt::CaseInsensitive) == 0) {
            return stripRegion(lang.englishName);
        }
    }
    // Unknown to our table (Google supports 100+ languages): try the base subtag.
    const qsizetype dash = normalized.indexOf(QLatin1Char('-'));
    if (dash > 0)
        return nameForProviderCode(normalized.left(dash));
    return normalized.toUpper();
}

bool Languages::isRightToLeft(const QString& code)
{
    const LanguageInfo* lang = find(code);
    return lang && lang->rightToLeft;
}

} // namespace hypers
