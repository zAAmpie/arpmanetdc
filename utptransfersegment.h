#ifndef UTPTRANSFERSEGMENT_H
#define UTPTRANSFERSEGMENT_H
#include "transfersegment.h"

class uTPTransferSegment : public TransferSegment
{
public:
    uTPTransferSegment(Transfer *parent = 0);
    ~uTPTransferSegment();

public slots:
    void incomingDataPacket(quint64 offset, QByteArray data);
    void transferTimerEvent();
    void setFileName(QString filename);
    void setFileSize(quint64 size);
    void startUploading();
    void startDownloading();
    qint64 getBytesReceivedNotFlushed();
};

#endif // UTPTRANSFERSEGMENT_H
