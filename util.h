#ifndef UTIL_H
#define UTIL_H
#include <QObject>
#include <QByteArray>
#include <QChar>
#include "base32.h"

typedef unsigned char byte;

QString base32Encode(byte *input, int inputLength);

QByteArray toQByteArray(quint16 n);
QByteArray toQByteArray(quint32 n);
QByteArray toQByteArray(quint64 n);

#endif // UTIL_H
