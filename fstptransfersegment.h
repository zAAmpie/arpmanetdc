#ifndef FSTPTRANSFERSEGMENT_H
#define FSTPTRANSFERSEGMENT_H
#include "transfersegment.h"

class FSTPTransferSegment : public TransferSegment
{
public:
    FSTPTransferSegment(QObject *parent = 0);
    ~FSTPTransferSegment();

public slots:
    void incomingDataPacket(quint64 offset, QByteArray data);
    void transferTimerEvent();
    void setFileName(QString filename);
    void setLastBucketSize(int size);
    void setLastBucketNumber(int n);
    void startUploading();
    void startDownloading();

private:
    inline void checkSendDownloadRequest(quint8 protocol, QHostAddress peer, QByteArray TTH,
                                         quint64 requestingOffset, quint64 requestingLength);
    inline int calculateBucketNumber(quint64 fileOffset);

    int status;
    quint64 requestingOffset;
    quint64 requestingLength;
    quint64 requestingTargetOffset;
};

#endif // FSTPTRANSFERSEGMENT_H
