#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "data/wadfile.h"
#include "data/doommap.h"

#include <QLabel>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    static MainWindow* get() { return instance; }

    void setMap(WADFile* nwad, DoomMap* nmap);
    WADFile* getMapWAD() { return wad; }
    DoomMap* getMap() { return map; }

    void resetScale();
    void setScale(float scale);

    void resetMouseXY();
    void setMouseXY(float x, float y);

private slots:
    void on_actionOpen_triggered();
    void on_actionNew_triggered();

private:
    Ui::MainWindow *ui;
    static MainWindow* instance;

    WADFile* wad;
    DoomMap* map;

    // status bar
    QLabel* statusStatus;
    QLabel* statusScale;
    QLabel* statusMouseXY;
};

#endif // MAINWINDOW_H
