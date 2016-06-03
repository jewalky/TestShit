#ifndef TEXMAN_H
#define TEXMAN_H

#include <QVector>
#include <QtGlobal>
#include <QImage>
#include "wadfile.h"

struct TexResource
{
    enum Type
    {
        WAD,
        ZIP,
        Directory
    };

    QString name;
    Type type;
};

class TexTexture
{
public:
    enum Type
    {
        Any, // not used with texture usually
        Flat,
        Texture,
        Graphic // this is something thats found randomly in the file. probably wont be used.
    };

    TexTexture(int w, int h, quint32* pixels, bool keeppixels = true)
    {
        width = w;
        height = h;
        this->pixels = pixels;
        this->keeppixels = keeppixels;
        texture = 0;
    }

    ~TexTexture()
    {
        if (pixels) delete pixels;
        pixels = 0;
    }

    static TexTexture* fromImage(QImage img);

    int getWidth() { return width; }
    int getHeight() { return height; }

    quint32 getPixelAt(int x, int y)
    {
        if (!pixels) return 0;
        if (x >= 0 && y >= 0 && x < width && y < height)
            return pixels[y+width+x];
        return 0;
    }

    int getTexture();

private:
    int texture;
    int width;
    int height;
    quint32* pixels;
    bool keeppixels;
};

void Tex_Reset();
void Tex_SetWADList(QVector<TexResource> wads); // this also initiates resource reload
void Tex_Reload();
TexTexture* Tex_GetTexture(QString name, TexTexture::Type preferredtype = TexTexture::Any, bool stricttype = false); // for classic doom, stricttype=true and type=Flat/Texture

// doom TEXTURE1/2 reader
struct DoomTexture1Patch
{
    int originx;
    int originy;
    QString name;
};

struct DoomTexture1Texture
{
    QString name;
    bool worldpanning;
    float scalex;
    float scaley;
    int width;
    int height;

    QVector<DoomTexture1Patch> patches;
};

QVector<DoomTexture1Texture> Tex_ReadTexture1(WADFile* wad, QString lumpname);

#endif // TEXMAN_H
