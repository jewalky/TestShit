#include "openmapdialog.h"
#include "ui_openmapdialog.h"

#include <QFileInfo>
#include "data/doommap.h"
#include <QMessageBox>
#include <QMetaEnum>
#include "mainwindow.h"
#include "data/texman.h"

OpenMapDialog::OpenMapDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OpenMapDialog)
{
    ui->setupUi(this);
}

OpenMapDialog::~OpenMapDialog()
{
    delete ui;
}

void OpenMapDialog::fromWAD(QString nfilename)
{
    filename = nfilename;
    QFileInfo info(filename);
    ui->label_wadName->setText(info.fileName());

    wad = WADFile::fromFile(filename);
    if (wad == 0)
    {
        hide();
        QMessageBox::critical(parentWidget(), "Error opening the WAD file", "File not found or not readable!");
        return;
    }

    if (!wad->isValid())
    {
        hide();
        QMessageBox::critical(parentWidget(), "Error opening the WAD file", wad->getError());
        delete wad;
        wad = 0;
        return;
    }

    QVector<DetectedDoomMap> maps = DoomMap::detectMaps(wad);
    //qDebug("%d map(s)", maps.size());
    for (int i = 0; i < maps.size(); i++)
    {
        //qDebug("map = %s", maps[i].name.toUtf8().data());
        QListWidgetItem* item = new QListWidgetItem();
        item->setData(Qt::UserRole, maps[i].name);
        item->setData(Qt::UserRole+1, maps[i].type);
        item->setText(maps[i].name+" ("+QMetaEnum::fromType<DoomMap::MapType>().valueToKey(maps[i].type)+")");
        ui->list_maps->addItem(item);
    }

    show();
}


void OpenMapDialog::on_buttonBox_accepted()
{
    // initialize map open
    // get map filename.
    QListWidgetItem* item = ui->list_maps->currentItem();
    if (!item)
    {
        delete wad;
        return;
    }

    QString mapname = item->data(Qt::UserRole).toString();
    DoomMap* map = new DoomMap(wad, mapname);
    MainWindow::get()->setMap(map);

    // init texture manager for this map.
    QVector<TexResource> resources = ui->resourceList->getResources();
    TexResource ownwad;
    ownwad.type = TexResource::WAD;
    ownwad.name = filename;
    resources.append(ownwad);
    Tex_SetWADList(resources);

    delete wad;
}

void OpenMapDialog::on_buttonBox_rejected()
{
    // do nothing
    delete wad;
}
