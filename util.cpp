#include "util.h"

// ------------------=====================   Sort STUKKENDE rubbish uit   =====================----------------------

// qbytearray append nie integers se rou data nie, hy maak droog.
// haastige implementering, kan later kyk watse potensiaal QVariant het om die drie funksies te herenig.
QByteArray toQByteArray(quint16 n)
{
    QByteArray a;
    quint16 mask = 0xff << 8;
    for (int i = 0; i < 16; i += 8)
    {
        QChar c = (char)((n & mask) >> (8 - i));
        mask >>= 8;
        a.append(c);
    }
    return a;
}

QByteArray toQByteArray(quint32 n)
{
    QByteArray a;
    quint32 mask = 0xff << 24;
    for (int i = 0; i < 32; i += 8)
    {
        QChar c = (char)((n & mask) >> (24 - i));
        mask >>= 8;
        a.append(c);
    }
    return a;
}

QByteArray toQByteArray(quint64 n)
{
    QByteArray a;
    //n = 0xff00ff00ff00ff00;
    quint64 mask = 0xff;
    mask <<= 56;
    for (int i = 0; i < 64; i += 8)
    {
        QChar c = (char)((n & mask) >> (56 - i));
        mask >>= 8;
        a.append(c);
    }
    return a;
}

