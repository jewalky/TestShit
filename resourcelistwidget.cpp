#include "resourcelistwidget.h"
#include "ui_resourcelistwidget.h"

#include "resourceeditdialog.h"

#include <QListWidget>
#include <QListWidgetItem>

ResourceListWidget::ResourceListWidget(QWidget *parent) :
    QGroupBox(parent),
    ui(new Ui::ResourceListWidget)
{
    ui->setupUi(this);
}

ResourceListWidget::~ResourceListWidget()
{
    clear();
    delete ui;
}

void ResourceListWidget::on_bAddResource_clicked()
{
    ResourceEditDialog* red = new ResourceEditDialog(this);
    connect(red, SIGNAL(accepted()), this, SLOT(handleNewAccepted()));
    red->show();
}

void ResourceListWidget::handleNewAccepted()
{
    ResourceEditDialog* red = (ResourceEditDialog*)sender();
    // todo extract filename from red
    TexResource res = red->getResource();
    if (res.name.isEmpty())
        return; // do nothing
    TexResource* savedres = new TexResource(res);
    QListWidgetItem* saveditem = new QListWidgetItem();
    saveditem->setText(res.name+" ("+getStringTypeFromType(res.type)+")");
    saveditem->setData(Qt::UserRole, qVariantFromValue((void*)savedres));
    ui->resourceList->addItem(saveditem);
}

void ResourceListWidget::handleEditAccepted()
{
    ResourceEditDialog* red = (ResourceEditDialog*)sender();
    TexResource res = red->getResource();
    if (res.name.isEmpty())
        return; // do nothing
    TexResource* savedres = new TexResource(res);
    // find currently selected item
    QListWidgetItem* saveditem = ui->resourceList->currentItem();

    if (!saveditem)
    {
        saveditem = new QListWidgetItem();
        ui->resourceList->addItem(saveditem);
    }
    else
    {
        TexResource* oldres = (TexResource*)saveditem->data(Qt::UserRole).value<void*>();
        delete oldres;
    }

    saveditem->setText(res.name+" ("+getStringTypeFromType(res.type)+")");
    saveditem->setData(Qt::UserRole, qVariantFromValue((void*)savedres));
}

void ResourceListWidget::on_resourceList_currentRowChanged(int currentRow)
{
    bool canedit = (currentRow >= 0 && currentRow < ui->resourceList->count());
    ui->bEditResource->setEnabled(canedit);
    ui->bDelResource->setEnabled(canedit);
}

void ResourceListWidget::on_bEditResource_clicked()
{
    QListWidgetItem* item = ui->resourceList->currentItem();
    if (!item) return;

    TexResource* res = (TexResource*)item->data(Qt::UserRole).value<void*>();
    ResourceEditDialog* red = new ResourceEditDialog(this);
    red->setWindowTitle("Modify resource");
    red->setResource(*res);
    connect(red, SIGNAL(accepted()), this, SLOT(handleEditAccepted()));
    red->show();
}

void ResourceListWidget::clear()
{
    // delete all current resources
    for (int i = 0; i < ui->resourceList->count(); i++)
    {
        QListWidgetItem* item = ui->resourceList->item(i);
        TexResource* res = (TexResource*)item->data(Qt::UserRole).value<void*>();
        delete res;
    }

    ui->resourceList->clear();
}

QVector<TexResource> ResourceListWidget::getResources()
{
    QVector<TexResource> out;

    for (int i = 0; i < ui->resourceList->count(); i++)
    {
        QListWidgetItem* item = ui->resourceList->item(i);
        TexResource* res = (TexResource*)item->data(Qt::UserRole).value<void*>();
        out.append(*res);
    }

    return out;
}

void ResourceListWidget::setResources(QVector<TexResource> resources)
{
    clear();
    for (int i = 0; i < resources.size(); i++)
    {
        TexResource* savedres = new TexResource(resources[i]);
        QListWidgetItem* saveditem = new QListWidgetItem();
        saveditem->setText(resources[i].name+" ("+getStringTypeFromType(resources[i].type)+")");
        saveditem->setData(Qt::UserRole, qVariantFromValue((void*)savedres));
        ui->resourceList->addItem(saveditem);
    }
}
