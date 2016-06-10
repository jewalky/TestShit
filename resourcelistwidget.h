#ifndef RESOURCELISTWIDGET_H
#define RESOURCELISTWIDGET_H

#include <QGroupBox>
#include "data/texman.h"

namespace Ui {
class ResourceListWidget;
}

class ResourceListWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit ResourceListWidget(QWidget *parent = 0);
    ~ResourceListWidget();

    QVector<TexResource> getResources();
    void setResources(QVector<TexResource> resources);

    void clear();

public slots:
    void handleNewAccepted();
    void handleEditAccepted();

private slots:
    void on_bAddResource_clicked();
    void on_resourceList_currentRowChanged(int currentRow);
    void on_bEditResource_clicked();

private:
    Ui::ResourceListWidget *ui;

    QString getStringTypeFromType(TexResource::Type t)
    {
        switch (t)
        {
        case TexResource::WAD:
            return "WAD";
        case TexResource::Directory:
            return "Directory";
        case TexResource::ZIP:
            return "ZIP";
        }

        return "Unknown";
    }
};

#endif // RESOURCELISTWIDGET_H
