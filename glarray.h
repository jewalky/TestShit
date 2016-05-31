#ifndef GLARRAY_H
#define GLARRAY_H

#include <QtGlobal>
#include <QVector>

struct GLVertex
{
    GLVertex()
    {
        x = y = z = 0;
        r = g = b = a = 255;
        u = v = 0;
    }

    float x;
    float y;
    float z;
    quint8 r;
    quint8 g;
    quint8 b;
    quint8 a;
    float u;
    float v;
};

class GLArray
{
public:
    GLArray();
    ~GLArray();

    void update();
    void draw(int mode);
    void draw(int mode, int first, int count);

    QVector<GLVertex> vertices;
};

#endif // GLARRAY_H
