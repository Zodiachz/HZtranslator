#pragma once

#include "app/AppContext.h"

#include <QTimer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

namespace hypers {

class HistoryModel;

class HistoryPage : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPage(const AppContext& context, QWidget* parent = nullptr);

Q_SIGNALS:
    void openInTranslator(const QString& text);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void reload();
    void updateButtons();
    qint64 selectedId() const;
    int selectedRow() const;

    AppContext m_ctx;
    HistoryModel* m_model = nullptr;
    QTableView* m_table = nullptr;
    QLineEdit* m_search = nullptr;
    QLabel* m_count = nullptr;
    QPushButton* m_copyTranslation = nullptr;
    QPushButton* m_copyOriginal = nullptr;
    QPushButton* m_open = nullptr;
    QPushButton* m_delete = nullptr;
    QPushButton* m_clear = nullptr;
    QTimer m_searchDebounce;
    bool m_dirty = true;
};

} // namespace hypers
