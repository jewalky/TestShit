#ifndef GLARRAY_H
#define GLARRAY_H

#include <QtGlobal>
#include <QVector>

struct GLVertex
{
    GLVertex(float x = 0, float y = 0, float z = 0, float u = 0, float v = 0, quint8 r = 255, quint8 g = 255, quint8 b = 255, quint8 a = 255)
    {
        this->x = x;
        this->y = y;
        this->z = z;
        this->u = u;
        this->v = v;
        this->r = r;
        this->g = g;
        this->b = b;
        this->a = a;
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

    GLVertex getCenter()
    {
        if (!vertices.size()) return GLVertex();

        float x = 0;
        float y = 0;
        float z = 0;

        for (int i = 0; i < vertices.size(); i++)
        {
            x += vertices[i].x;
            y += vertices[i].y;
            z += vertices[i].z;
        }

        x /= vertices.size();
        y /= vertices.size();
        z /= vertices.size();

        return GLVertex(x, y, z);
    }
};

#endif // GLARRAY_H
