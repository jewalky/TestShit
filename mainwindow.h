#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "data/wadfile.h"
#include "data/doommap.h"

#include <QLabel>
#include <QGLWidget>

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

    void setMap(DoomMap* nmap);
    DoomMap* getMap() { return map; }

    void resetScale();
    void setScale(float scale);

    void resetMouseXY();
    void setMouseXY(float x, float y);

    void set3DMode(bool is3d);

    QGLWidget* getSharedGLWidget();

    float getMouseX();
    float getMouseY();

private slots:
    void on_actionOpen_triggered();
    void on_actionNew_triggered();

private:
    Ui::MainWindow *ui;
    static MainWindow* instance;

    DoomMap* map;

    // status bar
    QLabel* statusStatus;
    QLabel* statusScale;
    QLabel* statusMouseXY;
};

#endif // MAINWINDOW_H
