// C++/WinRT first: <windows.h> macros (GetCurrentTime, ...) must not leak into the projections.
#include <unknwn.h>
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

#include "platform/win/WindowsOcrEngine.h"

#include <QLoggingCategory>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

Q_LOGGING_CATEGORY(lcOcr, "hypers.ocr")

namespace hypers {

namespace {

namespace wf = winrt::Windows::Foundation;
namespace wg = winrt::Windows::Globalization;
namespace wgi = winrt::Windows::Graphics::Imaging;
namespace wmo = winrt::Windows::Media::Ocr;
namespace wss = winrt::Windows::Storage::Streams;

QString toQString(const winrt::hstring& value)
{
    return QString::fromWCharArray(value.c_str(), static_cast<qsizetype>(value.size()));
}

winrt::hstring toHString(const QString& value)
{
    return winrt::hstring(reinterpret_cast<const wchar_t*>(value.utf16()),
                          static_cast<winrt::hstring::size_type>(value.size()));
}

void ensureMultiThreadedApartment()
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (const winrt::hresult_error& e) {
        // RPC_E_CHANGED_MODE: thread already joined an STA - still usable for these calls.
        qCDebug(lcOcr) << "init_apartment:" << toQString(e.message());
    }
}

// Windows OCR is most accurate when glyphs are ~20-40 px tall. Screen text
// is often smaller, so small captures are upscaled; a small margin keeps
// characters touching the selection edge recognisable.
QImage prepareImage(const QImage& source, int maxDimension)
{
    QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const int longest = std::max(image.width(), image.height());
    double scale = std::clamp(1600.0 / std::max(1, longest), 1.0, 3.0);
    constexpr int kMargin = 12;
    const double limit = static_cast<double>(maxDimension - 2 * kMargin);
    if (longest * scale > limit)
        scale = limit / longest;
    if (std::abs(scale - 1.0) > 0.01) {
        image = image.scaled(std::max(1, qRound(image.width() * scale)), std::max(1, qRound(image.height() * scale)),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QImage padded(image.width() + 2 * kMargin, image.height() + 2 * kMargin, QImage::Format_ARGB32_Premultiplied);
    padded.fill(image.pixelColor(0, 0));
    QPainter painter(&padded);
    painter.drawImage(kMargin, kMargin, image);
    painter.end();
    return padded;
}

wgi::SoftwareBitmap toSoftwareBitmap(const QImage& image)
{
    // Format_ARGB32_Premultiplied is B,G,R,A in memory on little-endian = Bgra8 premultiplied.
    const int width = image.width();
    const int height = image.height();
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    std::vector<uint8_t> pixels(rowBytes * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y)
        std::memcpy(pixels.data() + rowBytes * static_cast<std::size_t>(y), image.constScanLine(y), rowBytes);

    wss::DataWriter writer;
    writer.WriteBytes(winrt::array_view<const uint8_t>(pixels.data(), static_cast<uint32_t>(pixels.size())));
    const wss::IBuffer buffer = writer.DetachBuffer();
    return wgi::SoftwareBitmap::CreateCopyFromBuffer(buffer, wgi::BitmapPixelFormat::Bgra8, width, height,
                                                     wgi::BitmapAlphaMode::Premultiplied);
}

struct RecognizedLine {
    QString text;
    float top = 0;
    float bottom = 0;
};

QString assembleText(const std::vector<RecognizedLine>& lines, bool mergeLines, bool noSpaceScript)
{
    QString out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const RecognizedLine& line = lines[i];
        if (i == 0) {
            out = line.text;
            continue;
        }
        if (!mergeLines) {
            out += QLatin1Char('\n') + line.text;
            continue;
        }
        const RecognizedLine& prev = lines[i - 1];
        const float avgHeight = ((prev.bottom - prev.top) + (line.bottom - line.top)) / 2.0f;
        const float gap = line.top - prev.bottom;
        if (avgHeight > 0 && gap > avgHeight * 0.9f) {
            out += QStringLiteral("\n\n") + line.text; // paragraph break
        } else if (out.endsWith(QLatin1Char('-')) && !line.text.isEmpty() && line.text.at(0).isLower()) {
            out.chop(1); // de-hyphenate "transla-\ntion"
            out += line.text;
        } else {
            out += (noSpaceScript ? QString() : QStringLiteral(" ")) + line.text;
        }
    }
    return out;
}

} // namespace

QString WindowsOcrEngine::installHint(const QString& languageTag)
{
    const QString tag = languageTag.isEmpty() ? QStringLiteral("en-US") : languageTag;
    return QStringLiteral(
               "Windows OCR needs a language pack with OCR support. Add the language in Settings › Time & language › "
               "Language & region, or run in an elevated PowerShell:\n"
               "Add-WindowsCapability -Online -Name \"Language.OCR~~~%1~0.0.1.0\"")
        .arg(tag);
}

OcrOutput WindowsOcrEngine::recognize(const QImage& image, const QString& languageTag, bool mergeLines)
{
    OcrOutput output;
    if (image.isNull() || image.width() < 2 || image.height() < 2) {
        output.error = QStringLiteral("The selected area is empty.");
        return output;
    }

    ensureMultiThreadedApartment();
    try {
        wmo::OcrEngine engine{nullptr};
        if (languageTag.isEmpty()) {
            engine = wmo::OcrEngine::TryCreateFromUserProfileLanguages();
        } else {
            const wg::Language language{toHString(languageTag)};
            if (wmo::OcrEngine::IsLanguageSupported(language))
                engine = wmo::OcrEngine::TryCreateFromLanguage(language);
        }
        if (!engine) {
            if (languageTag.isEmpty())
                output.error = QStringLiteral("None of your Windows display languages has OCR installed.\n") + installHint();
            else
                output.error = QStringLiteral("OCR for %1 is not installed.\n").arg(languageTag) + installHint(languageTag);
            return output;
        }

        const QImage prepared = prepareImage(image, static_cast<int>(wmo::OcrEngine::MaxImageDimension()));
        const wgi::SoftwareBitmap bitmap = toSoftwareBitmap(prepared);
        const wmo::OcrResult result = engine.RecognizeAsync(bitmap).get();

        std::vector<RecognizedLine> lines;
        for (const wmo::OcrLine& line : result.Lines()) {
            RecognizedLine recognized;
            recognized.text = toQString(line.Text()).trimmed();
            recognized.top = std::numeric_limits<float>::max();
            recognized.bottom = std::numeric_limits<float>::lowest();
            for (const wmo::OcrWord& word : line.Words()) {
                const wf::Rect rect = word.BoundingRect();
                recognized.top = std::min(recognized.top, rect.Y);
                recognized.bottom = std::max(recognized.bottom, rect.Y + rect.Height);
            }
            if (recognized.bottom < recognized.top)
                recognized.top = recognized.bottom = 0;
            if (!recognized.text.isEmpty())
                lines.push_back(std::move(recognized));
        }

        output.languageTag = toQString(engine.RecognizerLanguage().LanguageTag());
        const bool noSpaceScript = output.languageTag.startsWith(QLatin1String("ja"), Qt::CaseInsensitive)
            || output.languageTag.startsWith(QLatin1String("zh"), Qt::CaseInsensitive);
        output.text = assembleText(lines, mergeLines, noSpaceScript).trimmed();
        output.ok = true;
        qCInfo(lcOcr) << "Recognized" << lines.size() << "lines," << output.text.size() << "chars, language"
                      << output.languageTag;
    } catch (const winrt::hresult_error& e) {
        output.error = QStringLiteral("Windows OCR failed: %1 (0x%2)")
                           .arg(toQString(e.message()))
                           .arg(static_cast<quint32>(e.code().value), 8, 16, QLatin1Char('0'));
        qCWarning(lcOcr) << output.error;
    } catch (const std::exception& e) {
        output.error = QStringLiteral("Windows OCR failed: %1").arg(QString::fromLocal8Bit(e.what()));
        qCWarning(lcOcr) << output.error;
    }
    return output;
}

QList<OcrLanguage> WindowsOcrEngine::availableLanguages()
{
    QList<OcrLanguage> languages;
    try {
        for (const wg::Language& language : wmo::OcrEngine::AvailableRecognizerLanguages())
            languages.append({toQString(language.LanguageTag()), toQString(language.DisplayName())});
    } catch (const winrt::hresult_error& e) {
        qCWarning(lcOcr) << "Listing OCR languages failed:" << toQString(e.message());
    }
    return languages;
}

} // namespace hypers
