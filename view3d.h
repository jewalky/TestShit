#ifndef VIEW3D_H
#define VIEW3D_H

#include <QGLWidget>
#include <QTimer>
#include <QPoint>
#include <QRect>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QGLFramebufferObject>

#include <QGLShaderProgram>
#include <QGLShader>

#include "glarray.h"

class View3D : public QGLWidget
{
    Q_OBJECT

public:
    View3D(QWidget* parent = 0);

    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int width, int height);

    void initMap();

    virtual void mousePressEvent(QMouseEvent* e);
    virtual void mouseReleaseEvent(QMouseEvent* e);
    virtual void wheelEvent(QWheelEvent* e);

    virtual void keyPressEvent(QKeyEvent* e);
    virtual void keyReleaseEvent(QKeyEvent* e);

    void becameVisible();

    // hover enum.
    // includes some types that aren't done yet.
    enum HoverType
    {
        Hover_None,
        Hover_Floor,
        Hover_Ceiling,
        Hover_SidedefTop,
        Hover_SidedefMiddle,
        Hover_SidedefBottom,
        Hover_Thing,
        Hover_VertexTop,
        Hover_VertexBottom
    };

    float getRDist() { return rdist; }
    void setRDist(float rd) { if (rd < 100) rd = 100; if (rd > 32767) rd = 32767; rdist = rd; }

public slots:
    void repaintTimerHandler();

private:
    void render(int pass);
    void updateMouseAngle();
    void setPerspective(float fov);

    QTimer* repaintTimer;
    bool wasVisible;

    float rdist;

    float posX;
    float posY;
    float posZ;

    float angX;
    float angY;
    float angZ;

    int mouseXLast;
    int mouseYLast;

    bool moveForward;
    bool moveBackward;
    bool moveLeft;
    bool moveRight;

    bool running;

    HoverType hoverType;
    int hoverId;

    QGLFramebufferObject* hoverFBO;

    // mouseover shader. I'm too lazy to make gltexenvi calls, especially that two colors need to be added.
    QGLShaderProgram highlightShader;
    QGLShaderProgram midtexSelectShader;

    void initShader(QString name, QGLShaderProgram& out, QString filenamevx, QString filenamefr);

    // unlike view2d, here it's used only for the crosshair
    void renderOverlay();
    void setClipRect(int x, int y, int w, int h);

    //
    friend class ScheduledObject;
    friend class ScheduledSidedef;
    friend class ScheduledThing;

    bool cullArray(GLArray& a);
    QMatrix4x4 modelview;
    QMatrix4x4 projection;
};

#endif // VIEW3D_H
