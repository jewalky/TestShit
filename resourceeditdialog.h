#ifndef RESOURCEEDITDIALOG_H
#define RESOURCEEDITDIALOG_H

#include <QDialog>
#include "data/texman.h"

namespace Ui {
class ResourceEditDialog;
}

class ResourceEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResourceEditDialog(QWidget *parent = 0);
    ~ResourceEditDialog();

    void setResource(TexResource& res);
    TexResource getResource();

private:
    Ui::ResourceEditDialog *ui;
};

#endif // RESOURCEEDITDIALOG_H
