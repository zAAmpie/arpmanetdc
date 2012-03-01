/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTPTRANSFERSEGMENT_H
#define UTPTRANSFERSEGMENT_H

#define FORCEINLINE inline

#include "transfersegment.h"

#include "util.h"

#include "libutp/utp.h"
#include "libutp/utp_utils.h"
#include "libutp/templates.h"

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
    void pauseDownload();
    void unpauseDownload();
    void abortTransfer();

private:
    UTPSocket *utpSocket;
    sockaddr_in addr;
    UTPFunctionTable utp_callbacks;
    
    const unsigned char * fileMap;
    qint64 segmentOffset;

    bool connect_called;

    // uTP callback functions
    void uTPRead(const byte *bytes, size_t count);
    void uTPWrite(byte *bytes, size_t count);
    size_t uTPGetRBSize();
    void uTPState(int state);
    void uTPError(int errcode);
    void uTPOverhead(bool send, size_t count, int type);

    void uTPSendTo(const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen);
    void uTPIncomingConnection(UTPSocket *s);

    // static uTP callback wrappers
    static void utp_read(void* data, const byte* bytes, size_t count);
    static void utp_write(void* data, byte* bytes, size_t count);
    static size_t utp_get_rb_size(void* data);
    static void utp_state(void* data, int state);
    static void utp_error(void* data, int errcode);
    static void utp_overhead(void *data, bool send, size_t count, int type);

    static void utp_sendto(void *data, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen);
    static void utp_incoming(void *data, UTPSocket *s);
};

#endif // UTPTRANSFERSEGMENT_H
