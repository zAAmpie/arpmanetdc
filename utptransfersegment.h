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
#include "transfersegment.h"

#define FORCEINLINE inline

// -------========== Borrow these from utptest, can strip and change to suit needs ========-----------

#ifdef WIN32
#define _CRT_SECURE_NO_DEPRECATE
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

#ifdef WIN32
// newer versions of MSVC define these in errno.h
#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#define EMSGSIZE WSAEMSGSIZE
#define ECONNREFUSED WSAECONNREFUSED
#define ECONNRESET WSAECONNRESET
#define ETIMEDOUT WSAETIMEDOUT
#endif
#endif

// platform-specific includes
#ifdef Q_WS_WIN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "libutp/win32_inet_ntop.h"
#else
//#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
//#include <arpa/inet.h>
#endif

#ifdef POSIX
typedef sockaddr_storage SOCKADDR_STORAGE;
#endif // POSIX

#include "libutp/utp.h"
#include "libutp/utp_utils.h"
#include "libutp/templates.h"

// These are for casting the options for getsockopt
// and setsockopt which if incorrect can cause these
// calls to fail.
#ifdef Q_WS_WIN
typedef char * SOCKOPTP;
typedef const char * CSOCKOPTP;
#else
typedef void * SOCKOPTP;
typedef const void * CSOCKOPTP;
#endif

struct socket_state
{
    socket_state(): total_sent(0), state(0), s(0) {}
    int total_sent;
    int state;
    UTPSocket* s;

    bool operator==(socket_state const& rhs) const { return s == rhs.s; }
};

#ifndef Q_WS_WIN
#define closesocket close
#define WSAGetLastError() errno
#define SOCKET int
#define INVALID_SOCKET -1
#endif

// --------------======================         ====================----------------


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

private:
    sockaddr_in sin;
    SOCKET sock;
    socket_state s;

    // uTP callback functions
    void uTPRead(const byte *bytes, size_t count);
    void uTPWrite(byte *bytes, size_t count);
    size_t uTPGetRBSize();
    void uTPState(int state);
    void uTPError(int errcode);
    void uTPOverhead(bool send, size_t count, int type);

    void uTPSendTo(const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen);
    void uTPIncomingConnection(UTPSocket *s);

#ifdef Q_WS_WIN
    WSADATA wsa;
#endif

    SOCKET make_socket(const struct sockaddr *addr, socklen_t addrlen);

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
