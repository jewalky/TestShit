#ifndef OPENMAPDIALOG_H
#define OPENMAPDIALOG_H

#include <QDialog>
#include "data/wadfile.h"

namespace Ui {
class OpenMapDialog;
}

class OpenMapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenMapDialog(QWidget *parent = 0);
    ~OpenMapDialog();

    void fromWAD(QString filename);

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

private:
    Ui::OpenMapDialog *ui;

    WADFile* wad;
};

#endif // OPENMAPDIALOG_H
