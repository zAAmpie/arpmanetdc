#ifndef UTIL_H
#define UTIL_H
#include <QObject>
#include <QByteArray>
#include <QChar>
#include "base32.h"

typedef unsigned char byte;
static const char *base32Alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

//Encoded an UNSIGNED CHAR *input into a QByteArray
QByteArray base32Encode(byte *input, int inputLength);
//Decodes a QByteArray into an UNSIGNED CHAR *output
//You need to call Base32DecodedLength to get outputLength before calling this function
bool base32Decode(QByteArray input, byte *output, int &outputLength);

//More intuitive base32 methods - returns true if conversion succeeded
bool base32Encode(QByteArray &data);
bool base32Decode(QByteArray &data);

QByteArray toQByteArray(quint16 n);
QByteArray toQByteArray(quint32 n);
QByteArray toQByteArray(quint64 n);

#endif // UTIL_H
