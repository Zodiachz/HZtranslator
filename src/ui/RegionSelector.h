#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>

namespace hypers {

class SelectionOverlay;

// Snipping-tool style region picker. Freezes every monitor (one overlay per
// QScreen so mixed-DPI setups render crisply), lets the user drag a
// rectangle, and returns the selected pixels at native resolution.
class RegionSelector : public QObject {
    Q_OBJECT
public:
    explicit RegionSelector(QObject* parent = nullptr);
    ~RegionSelector() override;

    bool isActive() const;
    void start();
    void cancel();

Q_SIGNALS:
    // `image` is in physical pixels; `globalRect` in Qt logical coordinates.
    void regionSelected(const QImage& image, const QRect& globalRect);
    void canceled();

private:
    void onOverlaySelected(SelectionOverlay* overlay, const QRect& localRect);
    void closeOverlays();

    QList<QPointer<SelectionOverlay>> m_overlays;
};

} // namespace hypers
