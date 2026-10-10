#pragma once

#include "core/AppSettings.h"
#include "core/HistoryStore.h"
#include "core/Languages.h"
#include "translation/TranslationService.h"

namespace hypers {

// "DEEPL · ENGLISH → ARABIC" style caption for popups.
inline QString describeTranslation(const TranslationService* service, const TranslationReply* reply)
{
    QString provider = reply->providerId();
    if (service) {
        if (const ITranslationProvider* p = service->provider(reply->providerId()))
            provider = p->displayName();
    }
    QString source = Languages::nameForProviderCode(reply->result().detectedSourceLanguage);
    if (source.isEmpty() && reply->request().sourceLanguage != QLatin1String(Languages::kAuto))
        source = Languages::nameForProviderCode(reply->request().sourceLanguage);
    const LanguageInfo* target = Languages::find(reply->request().targetLanguage);
    const QString targetName = target ? target->englishName : reply->request().targetLanguage;
    QString title = provider + QStringLiteral("  ·  ");
    if (!source.isEmpty())
        title += source + QStringLiteral(" → ");
    title += targetName;
    if (reply->result().fromCache)
        title += QStringLiteral("  ·  cached");
    return title;
}

inline void recordHistory(HistoryStore* history, const AppSettings* settings, const QString& origin,
                          const QString& application, const TranslationReply* reply)
{
    if (!history || !settings || !settings->historyEnabled() || !reply->isSuccess())
        return;
    HistoryEntry entry;
    entry.origin = origin;
    entry.application = application;
    entry.sourceText = reply->request().text;
    entry.translatedText = reply->result().translatedText;
    entry.sourceLanguage = reply->result().detectedSourceLanguage.isEmpty()
        ? reply->request().sourceLanguage
        : reply->result().detectedSourceLanguage;
    entry.targetLanguage = reply->request().targetLanguage;
    entry.provider = reply->providerId();
    history->add(entry);
}

} // namespace hypers
