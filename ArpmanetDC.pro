#-------------------------------------------------
#
# Project created by QtCreator 2011-11-08T09:14:58
#
#-------------------------------------------------

QT       += core gui network

LIBS += /usr/lib64/libsqlite3.so /usr/lib64/libcryptopp.so /usr/local/lib/libutp.a /lib64/librt.so.1

TARGET = ArpmanetDC
TEMPLATE = app


SOURCES += main.cpp\
        arpmanetdc.cpp \
    hubconnection.cpp \
    dispatcher.cpp \
    networkbootstrap.cpp \
    networktopology.cpp \
    util.cpp \
    transfermanager.cpp \
    transfer.cpp \
    uploadtransfer.cpp \
    downloadtransfer.cpp \
    sharewidget.cpp \
    hashfilethread.cpp \
    sharesearch.cpp \
    parsedirectorythread.cpp \
    searchwidget.cpp \
    pmwidget.cpp \
    execthread.cpp \
    checkableproxymodel.cpp \
    base32.cpp \
    customtableitems.cpp \
    delayedexecutiontimer.cpp \
    downloadqueuewidget.cpp \
    settingswidget.cpp \
    downloadfinishedwidget.cpp \
    helpwidget.cpp \
    resourceextractor.cpp \
    transferwidget.cpp \
    transfersegment.cpp \
    fstptransfersegment.cpp \
    bucketflushthread.cpp \
    utptransfersegment.cpp

HEADERS  += arpmanetdc.h \
    hubconnection.h \
    dispatcher.h \
    networkbootstrap.h \
    networktopology.h \
    util.h \
    transfermanager.h \
    transfer.h \
    uploadtransfer.h \
    downloadtransfer.h \
    execthread.h \
    sharesearch.h \
    sharewidget.h \
    hashfilethread.h \
    parsedirectorythread.h \
    pmwidget.h \
    searchwidget.h \
    downloadqueuewidget.h \
    checkableproxymodel.h \
    base32.h \
    customtableitems.h \
    delayedexecutiontimer.h \
    downloadfinishedwidget.h \
    settingswidget.h \
    helpwidget.h \
    resourceextractor.h \
    transferwidget.h \
    protocoldef.h \
    transfersegment.h \
    fstptransfersegment.h \
    bucketflushthread.h \
    utptransfersegment.h

#FORMS    += arpmanetdc.ui

RESOURCES += \
    arpmanetdc.qrc


