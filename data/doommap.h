#ifndef DOOMMAP_H
#define DOOMMAP_H

#include <QObject>
#include <QVector>
#include <QIODevice>
#include "wadfile.h"
#include <QMap>
#include <QVariant>
#include "../glarray.h"

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
class DoomMapComponent : public QMap<QString, QVariant>
{
public:
    DoomMapComponent(DoomMap* parent)
    {
        this->parent = parent;
        (*this)["comment"] = "";
    }

    DoomMap* getParent() { return parent; }

private:
    DoomMap* parent;
};

class DoomMapVertex : public DoomMapComponent
{
public:
    DoomMapVertex() : DoomMapComponent(0) {}
    DoomMapVertex(DoomMap* parent) : DoomMapComponent(parent)
    {
        (*this)["x"] = 0;
        (*this)["y"] = 0;
    }
};

class DoomMapLinedef : public DoomMapComponent
{
public:
    DoomMapLinedef() : DoomMapComponent(0) {}
    DoomMapLinedef(DoomMap* parent) : DoomMapComponent(parent)
    {
        (*this)["id"] = 0;
        (*this)["v1"] = -1;
        (*this)["v2"] = -1;

        //
        (*this)["blocking"] = false;
        (*this)["blockmonsters "] = false;
        (*this)["twosided"] = false;
        (*this)["dontpegtop"] = false;
        (*this)["dontpegbottom"] = false;
        (*this)["secret"] = false;
        (*this)["blocksound"] = false;
        (*this)["dontdraw"] = false;
        (*this)["mapped"] = false;

        //
        (*this)["passuse"] = false;

        //
        (*this)["translucent"] = false;
        (*this)["jumpover"] = false;
        (*this)["blockfloaters"] = false;

        //
        (*this)["playercross"] = false;
        (*this)["playeruse"] = false;
        (*this)["monstercross"] = false;
        (*this)["monsteruse"] = false;
        (*this)["impact"] = false;
        (*this)["playerpush"] = false;
        (*this)["monsterpush"] = false;
        (*this)["missilecross"] = false;
        (*this)["repeatspecial"] = false;

        //
        (*this)["special"] = 0;
        (*this)["arg0"] = 0;
        (*this)["arg1"] = 0;
        (*this)["arg2"] = 0;
        (*this)["arg3"] = 0;
        (*this)["arg4"] = 0;

        //
        (*this)["sidefront"] = -1;
        (*this)["sideback"] = -1;
    }

    DoomMapVertex* getV1(DoomMapSector* sector = 0)
    {
        if (sector && getFront() && getFront()->getSector() != sector)
            return getV2();
        int v1id = (*this)["v1"].toInt();
        if (v1id < 0 || v1id >= getParent()->vertices.size())
            return 0;
        return &getParent()->vertices[v1id];
    }

    DoomMapVertex* getV2(DoomMapSector* sector = 0)
    {
        if (sector && getFront() && getFront()->getSector() != sector)
            return getV1();
        int v2id = (*this)["v2"].toInt();
        if (v2id < 0 || v2id >= getParent()->vertices.size())
            return 0;
        return &getParent()->vertices[v2id];
    }

    DoomMapSidedef* getFront()
    {
        int sidefront = (*this)["sidefront"].toInt();
        if (sidefront < 0 || sidefront >= getParent()->sidedefs.size())
            return 0;
        return &getParent()->sidedefs[sidefront];
    }

    DoomMapSidedef* getBack()
    {
        int sideback = (*this)["sideback"].toInt();
        if (sideback < 0 || sideback >= getParent()->sidedefs.size())
            return 0;
        return &getParent()->sidedefs[sideback];
    }
};

class DoomMapSidedef : public DoomMapComponent
{
public:
    DoomMapSidedef() : DoomMapComponent(0) {}
    DoomMapSidedef(DoomMap* parent) : DoomMapComponent(parent)
    {
        (*this)["offsetx"] = 0;
        (*this)["offsety"] = 0;

        (*this)["texturetop"] = "-";
        (*this)["texturebottom"] = "-";
        (*this)["texturemiddle"] = "-";

        (*this)["sector"] = -1;
    }

    DoomMapSector* getSector()
    {
        int sid = (*this)["sector"].toInt();
        if (sid < 0 || sid >= getParent()->sectors.size())
            return 0;
        return &getParent()->sectors[sid];
    }
};

class DoomMapSector : public DoomMapComponent
{
public:
    DoomMapSector() : DoomMapComponent(0) {}
    DoomMapSector(DoomMap* parent) : DoomMapComponent(parent)
    {
        (*this)["heightfloor"] = 0;
        (*this)["heightceiling"] = 0;

        (*this)["texturefloor"] = "-";
        (*this)["textureceiling"] = "-";

        (*this)["lightlevel"] = 160;

        (*this)["special"] = 0;
        (*this)["id"] = 0;
    }

    // generate sector triangles
    void triangulate();

    GLArray triangles; // this is by default at height 0, to be used with glTranslate if needed for floor/ceiling
};

#endif // DOOMMAP_H
