#ifndef DOOMMAP_H
#define DOOMMAP_H

#include <QObject>
#include <QVector>
#include <QIODevice>
#include "wadfile.h"
#include <QMap>
#include <QVariant>
#include "../glarray.h"
#include <QPolygonF>

// map formats
// 1) Doom
// 2) Strife
// 3) Hexen
// 4) UDMF

struct DetectedDoomMap;
class DoomMapVertex;
class DoomMapLinedef;
class DoomMapSidedef;
class DoomMapSector;
class DoomMap : public QObject
{
    Q_OBJECT

public:
    enum MapType
    {
        Doom,
        Strife,
        Hexen,
        UDMF
    };

    Q_ENUM(MapType)

    DoomMap();
    DoomMap(WADFile* wad, QString name);

    //
    static QVector<DetectedDoomMap> detectMaps(WADFile* wad);

    // map fields!
    QVector<DoomMapVertex> vertices;
    QVector<DoomMapLinedef> linedefs;
    QVector<DoomMapSidedef> sidedefs;
    QVector<DoomMapSector> sectors;

private:
    MapType type;

    QByteArray behavior;
    QString scripts;

    void initUDMF(QString text);
    void initClassic(QIODevice* things, QIODevice* linedefs, QIODevice* sidedefs, QIODevice* vertexes, QIODevice* sectors);
};

struct DetectedDoomMap
{
    QString name;
    DoomMap::MapType type;
};

// https://github.com/rheit/zdoom/blob/master/specs/udmf.txt
class DoomMapComponent
{
public:
    DoomMapComponent(DoomMap* parent)
    {
        this->parent = parent;
        properties["comment"] = "";
    }

    DoomMap* getParent() { return parent; }
    QMap<QString, QVariant>& getProperties() { return properties; }

private:
    DoomMap* parent;
    QMap<QString, QVariant> properties;
};

class DoomMapVertex : public DoomMapComponent
{
public:
    float x;
    float y;

    DoomMapVertex() : DoomMapComponent(0) {}
    DoomMapVertex(DoomMap* parent) : DoomMapComponent(parent)
    {
        x = 0;
        y = 0;
    }
};

class DoomMapSidedef : public DoomMapComponent
{
public:
    int offsetx;
    int offsety;

    QString texturetop;
    QString texturebottom;
    QString texturemiddle;

    int sector;

    DoomMapSidedef() : DoomMapComponent(0) {}
    DoomMapSidedef(DoomMap* parent) : DoomMapComponent(parent)
    {
        offsetx = offsety = 0;
        texturetop = texturebottom = texturemiddle = "-";
        sector = -1;
    }

    DoomMapSector* getSector()
    {
        if (sector < 0 || sector >= getParent()->sectors.size())
            return 0;
        return &getParent()->sectors[sector];
    }
};

class DoomMapLinedef : public DoomMapComponent
{
public:
    int id;
    int v1;
    int v2;

    bool blocking;
    bool blockmonsters;
    bool twosided;
    bool dontpegtop;
    bool dontpegbottom;
    bool secret;
    bool blocksound;
    bool dontdraw;
    bool mapped;

    bool passuse;

    bool translucent;
    bool jumpover;
    bool blockfloaters;

    bool playercross;
    bool playeruse;
    bool monstercross;
    bool monsteruse;
    bool impact;
    bool playerpush;
    bool monsterpush;
    bool missilecross;
    bool repeatspecial;

    int special;
    int arg0;
    int arg1;
    int arg2;
    int arg3;
    int arg4;

    int sidefront;
    int sideback;

    DoomMapLinedef() : DoomMapComponent(0) {}
    DoomMapLinedef(DoomMap* parent) : DoomMapComponent(parent)
    {
        id = 0;
        v1 = -1;
        v2 = -1;

        blocking = false;
        blockmonsters = false;
        twosided = false;
        dontpegtop = false;
        dontpegbottom = false;
        secret = false;
        blocksound = false;
        dontdraw = false;
        mapped = false;

        passuse = false;

        translucent = false;
        jumpover = false;
        blockfloaters = false;

        playercross = false;
        playeruse = false;
        monstercross = false;
        monsteruse = false;
        impact = false;
        playerpush = false;
        monsterpush = false;
        missilecross = false;
        repeatspecial = false;

        special = 0;
        arg0 = arg1 = arg2 = arg3 = arg4 = 0;

        sidefront = sideback = -1;
    }

    DoomMapVertex* getV1(DoomMapSector* sector = 0)
    {
        if (sector && getFront() && getFront()->getSector() != sector)
            return getV2();
        if (v1 < 0 || v1 >= getParent()->vertices.size())
            return 0;
        return &getParent()->vertices[v1];
    }

    DoomMapVertex* getV2(DoomMapSector* sector = 0)
    {
        if (sector && getFront() && getFront()->getSector() != sector)
            return getV1();
        if (v2 < 0 || v2 >= getParent()->vertices.size())
            return 0;
        return &getParent()->vertices[v2];
    }

    DoomMapSidedef* getFront()
    {
        if (sidefront < 0 || sidefront >= getParent()->sidedefs.size())
            return 0;
        return &getParent()->sidedefs[sidefront];
    }

    DoomMapSidedef* getBack()
    {
        if (sideback < 0 || sideback >= getParent()->sidedefs.size())
            return 0;
        return &getParent()->sidedefs[sideback];
    }
};

class DoomMapSector : public DoomMapComponent
{
public:
    int heightfloor;
    int heightceiling;

    QString texturefloor;
    QString textureceiling;

    int lightlevel;

    int special;
    int id;

    DoomMapSector() : DoomMapComponent(0) {}
    DoomMapSector(DoomMap* parent) : DoomMapComponent(parent)
    {
        heightfloor = heightceiling = 0;
        texturefloor = textureceiling = "-";
        lightlevel = 160;
        special = id = 0;
    }

    // generate sector triangles
    void triangulate();

    GLArray triangles; // this is by default at height 0, to be used with glTranslate if needed for floor/ceiling
};

class SectorPolygonTracer
{
public:
    SectorPolygonTracer(DoomMapSector* sector)
    {
        this->sector = sector;
        linedefs.clear();

        DoomMap* p = sector->getParent();
        for (int i = 0; i < p->linedefs.size(); i++)
        {
            DoomMapSidedef* front = p->linedefs[i].getFront();
            DoomMapSidedef* back = p->linedefs[i].getBack();

            bool hfront = (front && front->getSector() == sector);
            bool hback = (back && back->getSector() == sector);

            if (hfront && hback)
                continue;

            if (hfront || hback)
                linedefs.append(&p->linedefs[i]);
        }
    }

    // returns linedefs that refer to this sector with one of their sidedefs
    QVector<DoomMapLinedef*> getLinedefs()
    {
        return linedefs;
    }

    // returns a list of separate line loops in this sector.
    QVector<QPolygonF> getPolygons()
    {
        QVector<QPolygonF> polygons;
        while (true)
        {
            // find first unchecked line.
            DoomMapLinedef* nextld = 0;
            for (int i = 0; i < linedefs.size(); i++)
            {
                if (checkedlinedefs.contains(linedefs[i]))
                    continue;
                nextld = linedefs[i];
                break;
            }

            if (!nextld)
                break;

            QPair< QPolygonF, QVector<DoomMapLinedef*> > poly = nextPolygon(nextld);
            if (poly.first.size() <= 0)
                break;
            polygons.append(poly.first);
            for (int i = 0; i < poly.second.size(); i++)
                checkedlinedefs.append(poly.second[i]);
        }

        return polygons;
    }

private:

    DoomMapSector* sector;

    // a list of linedefs in the reference sector.
    QVector<DoomMapLinedef*> linedefs;

    // a list of linedefs that have already been traversed.
    QVector<DoomMapLinedef*> checkedlinedefs;

    // returns a pair of polygon and lines that this polygon consists of.
    // if no polygon found, returns two empty objects.
    // currentcheckedlinedefs is used to prevent infinite recursion.
    QPair< QPolygonF, QVector<DoomMapLinedef*> > nextPolygon(DoomMapLinedef* line);
};

#endif // DOOMMAP_H
