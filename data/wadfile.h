#ifndef WADFILE_H
#define WADFILE_H

#include <QString>
#include <QIODevice>
#include <QVector>

class WADEntry
{
public:
    WADEntry(QString name, int offset, QByteArray data)
    {
        this->name = name;
        this->offset = offset;
        this->data = data;
    }

    QString getName() { return name; }
    int getOffset() { return offset; }
    QByteArray& getData() { return data; }

private:
    QString name;
    int offset;
    QByteArray data;
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

    int getNumForName(QString name, int num = 0);
    int getLastNumForName(QString name, int num = 2147483647);
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
