#ifndef WADFILE_H
#define WADFILE_H

#include <QString>
#include <QIODevice>
#include <QVector>

enum WADNamespace
{
    NS_Global,
    NS_Sprites,
    NS_Flats,
    NS_Colormaps,
    NS_ACS,
    NS_Textures,
    NS_Voices,
    NS_Hires,
    NS_Sounds,
    NS_Patches,
    NS_Graphics,
    NS_Music,
    NS_Skin,
    NS_Voxels,
    NS_Any = -1
};

class WADEntry
{
public:
    WADEntry(QString name, int offset, WADNamespace ns, QByteArray data)
    {
        this->name = name;
        this->offset = offset;
        this->ns = ns;
        this->data = data;
    }

    QString getName() { return name; }
    int getOffset() { return offset; }
    WADNamespace getNamespace() { return ns; }
    QByteArray& getData() { return data; }

private:
    QString name;
    int offset;
    QByteArray data;
    WADNamespace ns;
};

class WADFile
{
private:
    WADFile(QIODevice* device);

public:
    static WADFile* fromFile(QString filename);
    static WADFile* fromStream(QIODevice* device);

    bool isValid() { return valid; }
    QString getError() { return error; }

    WADEntry* getEntry(int num);
    WADEntry* removeEntry(int num);
    void putEntry(int num, WADEntry* ent);

    int getNumForName(QString name, int num = 0, WADNamespace ns = NS_Any);
    int getLastNumForName(QString name, int num = -1, WADNamespace ns = NS_Any);
    int getSize() { return entries.size(); }

private:
    QVector<WADEntry*> entries;
    bool valid;
    QString error;

    void setError(QString e)
    {
        valid = false;
        error = e;
    }
};

#endif // WADFILE_H
