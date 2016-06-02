#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include "openmapdialog.h"

MainWindow* MainWindow::instance = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    instance = this;

    map = 0;
    wad = 0;

    //
    QFont statusFont = font();
    statusFont.setBold(true);
    QFontMetrics statusFontMetrics(statusFont);

    statusStatus = new QLabel(this);
    statusStatus->setContentsMargins(4, 2, 4, 2);
    statusStatus->setFont(statusFont);
    statusScale = new QLabel(this);
    statusScale->setContentsMargins(4, 2, 4, 2);
    statusScale->setAlignment(Qt::AlignCenter);
    statusScale->setFont(statusFont);
    statusScale->setFixedWidth(statusFontMetrics.width("9000%")+32);
    statusMouseXY = new QLabel(this);
    statusMouseXY->setContentsMargins(4, 2, 4, 2);
    statusMouseXY->setAlignment(Qt::AlignCenter);
    statusMouseXY->setFont(statusFont);
    statusMouseXY->setFixedWidth(statusFontMetrics.width("99999 , 99999")+64);
    statusBar()->addWidget(statusStatus, 1);
    statusBar()->addWidget(statusScale);
    statusBar()->addWidget(statusMouseXY);
    statusStatus->setText("Started.");
    resetScale();
    resetMouseXY();
}

MainWindow::~MainWindow()
{
    if (map) delete map;
    if (wad) delete wad;
    map = 0;
    wad = 0;
    delete ui;
    ui = 0;
    instance = 0;
}

void MainWindow::on_actionNew_triggered()
{

}

void MainWindow::on_actionOpen_triggered()
{
    // input: WAD file
    QString fileName = QFileDialog::getOpenFileName(this, "Open Map", "", "WAD Files (*.wad)");
    if (fileName.isEmpty())
        return;
    OpenMapDialog* mapsdlg = new OpenMapDialog(this);
    mapsdlg->fromWAD(fileName);
}

void MainWindow::setMap(WADFile* nwad, DoomMap* nmap)
{
    if (map != nmap && map)
        delete map;
    map = nmap;
    if (wad != nwad && wad)
        delete wad;
    wad = nwad;

    // process stuff
    ui->view2d->initMap();
    ui->view3d->initMap();
}

void MainWindow::resetScale()
{
    //statusScale->setText("--");
    setScale(1);
}

void MainWindow::setScale(float scale)
{
    int scale100 = scale*100;
    if (scale100 < 1) scale100 = 1;
    statusScale->setText(QString::number(scale100)+"%");
}

void MainWindow::resetMouseXY()
{
    statusMouseXY->setText("-- , --");
}

void MainWindow::setMouseXY(float x, float y)
{
    statusMouseXY->setText(QString::number((int)x)+" , "+QString::number((int)y));
}

void MainWindow::set3DMode(bool is3d)
{
    ui->modeStack->setCurrentWidget(is3d?ui->page3d:ui->page2d);
}
