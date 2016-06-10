#include "resourceeditdialog.h"
#include "ui_resourceeditdialog.h"

ResourceEditDialog::ResourceEditDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ResourceEditDialog)
{
    ui->setupUi(this);
}

ResourceEditDialog::~ResourceEditDialog()
{
    delete ui;
}

void ResourceEditDialog::setResource(TexResource& res)
{
    switch (res.type)
    {
    case TexResource::WAD:
        ui->tabWidget->setCurrentIndex(0);
        ui->wad_fileName->setText(res.name);
        ui->wad_strictPatches->setChecked(res.wad_strictPatches);
        break;

    case TexResource::Directory:
        ui->tabWidget->setCurrentIndex(1);
        ui->dir_dirName->setText(res.name);
        ui->dir_rootTextures->setChecked(res.dir_rootTextures);
        ui->dir_rootFlats->setChecked(res.dir_rootFlats);
        break;

    case TexResource::ZIP:
        ui->tabWidget->setCurrentIndex(2);
        ui->pk3_fileName->setText(res.name);
        break;
    }

    ui->chk_excludeFromTesting->setChecked(res.exclude);
}

TexResource ResourceEditDialog::getResource()
{
    TexResource res;

    res.exclude = ui->chk_excludeFromTesting->isChecked();

    if (ui->tabWidget->currentIndex() == 0) // wad file
    {
        res.type = TexResource::WAD;
        res.name = ui->wad_fileName->text();
        res.wad_strictPatches = ui->wad_strictPatches->isChecked();
        return res;
    }
    else if (ui->tabWidget->currentIndex() == 1) // directory
    {
        res.type = TexResource::Directory;
        res.name = ui->dir_dirName->text();
        res.dir_rootTextures = ui->dir_rootTextures->isChecked();
        res.dir_rootFlats = ui->dir_rootFlats->isChecked();
        return res;
    }
    else if (ui->tabWidget->currentIndex() == 2) // pk3
    {
        res.type = TexResource::ZIP;
        res.name = ui->pk3_fileName->text();
        return res;
    }

    return res;
}
