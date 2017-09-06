TEMPLATE = lib
CONFIG += dll

QT += widgets

macx {
    CONFIG -= dll
    CONFIG += lib_bundle
}

CONFIG(debug, debug|release) {
    TARGET = qzintd
} else {
    TARGET = qzint
}

INCLUDEPATH += ../backend

DEFINES += ZINT_VERSION="\\\"2.6.0\\\""
DEFINES += QZINTLIB_LIBRARY

!contains(DEFINES, NO_PNG) {
    INCLUDEPATH += $$PWD/../extern/libpng/include
    INCLUDEPATH += $$PWD/../extern/zlib/include
    equals(QT_ARCH, x86_64) {
        target.path=$$PWD/lib64
        LIBS += -L$$PWD/../extern/libpng/lib/x64 \
                -L$$PWD/../extern/zlib/lib/x64
    } else {
        equals(QT_ARCH, i386) {
            target.path=$$PWD/lib32
            LIBS += -L$$PWD/../extern/libpng/lib/x86 \
                    -L$$PWD/../extern/zlib/lib/x86
        } else {
            warning("Unsupported platform: $$QT_ARCH")
        }
    }
    LIBS += -llibpng16 \
            -lzlib
}

contains(DEFINES, QR_SYSTEM) {
    LIBS += -lqrencode
}

contains(DEFINES, QR) {

INCLUDEPATH += qrencode

HEADERS += qrencode/bitstream.h \
           qrencode/mask.h \
           qrencode/qrencode.h \
           qrencode/qrencode_inner.h \
           qrencode/qrinput.h \
           qrencode/qrspec.h \
           qrencode/rscode.h \
           qrencode/split.h 

SOURCES += qrencode/bitstream.c \
           qrencode/mask.c \
           qrencode/qrencode.c \
           qrencode/qrinput.c \
           qrencode/qrspec.c \
           qrencode/rscode.c \
           qrencode/split.c 
}

HEADERS +=  ../backend/aztec.h \
            ../backend/bmp.h \
            ../backend/code49.h \
            ../backend/common.h \
            ../backend/composite.h \
            ../backend/dmatrix.h \
            ../backend/eci.h \
            ../backend/font.h \
            ../backend/gridmtx.h \
            ../backend/gs1.h \
            ../backend/hanxin.h \
            ../backend/large.h \
            ../backend/maxicode.h \
            ../backend/pcx.h \
            ../backend/pdf417.h \
            ../backend/reedsol.h \
            ../backend/rss.h \
            ../backend/sjis.h \
            ../backend/stdint_msvc.h \
            ../backend/zint.h \
            ../backend/code1.h \
            ../backend/emf.h \
            ../backend/gb2312.h \
            ../backend/gb18030.h \
            ../backend/ms_stdint.h \
            ../backend/qr.h \
            ../backend/tif.h \
            qzint.h \
    qzint_export.h

SOURCES += ../backend/2of5.c \
           ../backend/auspost.c \
           ../backend/aztec.c \
           ../backend/bmp.c \
           ../backend/codablock.c \
           ../backend/code.c \
           ../backend/code128.c \
           ../backend/code16k.c \
           ../backend/code49.c \
           ../backend/common.c \
           ../backend/composite.c \
           ../backend/dmatrix.c \
           ../backend/dotcode.c \
           ../backend/eci.c \
           ../backend/emf.c \
           ../backend/gif.c \
           ../backend/gridmtx.c \
           ../backend/gs1.c \
           ../backend/hanxin.c \
           ../backend/imail.c \
           ../backend/large.c \
           ../backend/library.c \
           ../backend/maxicode.c \
           ../backend/medical.c \
           ../backend/pcx.c \
           ../backend/pdf417.c \
           ../backend/plessey.c \
           ../backend/postal.c \
           ../backend/ps.c \
           ../backend/raster.c \
           ../backend/reedsol.c \
            ../backend/render.c \
           ../backend/rss.c \
           ../backend/svg.c \
           ../backend/telepen.c \
           ../backend/tif.c \
           ../backend/upcean.c \
           ../backend/qr.c \
           ../backend/dllversion.c \
           ../backend/code1.c \
           ../backend/png.c \
           qzint.cpp

#VERSION = 2.6.0

QMAKE_CFLAGS += /LD /MD

INSTALLS += target

