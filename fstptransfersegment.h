#ifndef FSTPTRANSFERSEGMENT_H
#define FSTPTRANSFERSEGMENT_H
#include "transfersegment.h"
#include "transfer.h"

#define FSTP_TRANSFER_MINIMUM_SEGMENT 65536
#define FSTP_TRANSFER_MAXIMUM_SEGMENT 524288

class Transfer;

class FSTPTransferSegment : public TransferSegment
{
public:
    FSTPTransferSegment(Transfer *parent = 0);
    ~FSTPTransferSegment();

public slots:
    void incomingDataPacket(quint64 offset, QByteArray data);
    void transferTimerEvent();
    void setFileName(QString filename);
    void setFileSize(quint64 size);
    void setLastBucketSize(int size);
    void setLastBucketNumber(int n);
    void startUploading();
    void startDownloading();

private:
    inline void checkSendDownloadRequest(quint8 protocol, QHostAddress peer, QByteArray TTH,
                                         quint64 requestingOffset, qint64 requestingLength);
    inline int calculateBucketNumber(quint64 fileOffset);

    int status;
    quint64 requestingOffset;
    quint64 requestingLength;
    quint64 requestingTargetOffset;
    int packetsSinceUpdate;
    int retransmitTimeoutCounter;
    int retransmitRetryCounter;

    Transfer *pParent;
};

#endif // FSTPTRANSFERSEGMENT_H
