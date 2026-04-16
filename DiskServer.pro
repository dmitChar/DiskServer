QT = core sql network concurrent

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        database/databasemanager.cpp \
        httpsserver.cpp \
        main.cpp \
    router.cpp \
    utils/jwtutils.cpp \
    middleware/authmiddleware.cpp \
    handlers/authhandler.cpp \
    handlers/filehandler.cpp \
    handlers/userhandler.cpp \
    utils/fileutils.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    database/databasemanager.h \
    httpsserver.h \
    router.h \
    utils/jwtutils.h \
    utils/response.h \
    utils/user.h \
    middleware/authmiddleware.h \
    handlers/authhandler.h \
    handlers/filehandler.h \
    handlers/userhandler.h \
    utils/hashutils.h \
    utils/fileutils.h \
    utils/filedata.h

RESOURCES += \
    resource.qrc
