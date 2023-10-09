CONFIG(debug, debug|release) {
    CONFIG -= debug release
    CONFIG += debug
}
CONFIG(release, debug|release) {
    CONFIG -= debug release
    CONFIG += release
}


TARGET = test_integration
TEMPLATE = app

CONFIG -= qt
CONFIG += object_parallel_to_source
CONFIG += console

DEFINES += MEGACMD_TESTING_CODE

win32 {
    LIBS +=  -lshlwapi -lws2_32
    LIBS +=  -lshell32 -luser32 -ladvapi32

    RC_FILE = icon.rc
    QMAKE_LFLAGS += /LARGEADDRESSAWARE
    QMAKE_LFLAGS_WINDOWS += /SUBSYSTEM:WINDOWS,5.01
    QMAKE_LFLAGS_CONSOLE += /SUBSYSTEM:CONSOLE,5.01
    DEFINES += PSAPI_VERSION=1
    DEFINES += UNICODE _UNICODE NTDDI_VERSION=0x06000000 _WIN32_WINNT=0x0600
    QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO
    QMAKE_LFLAGS_RELEASE = $$QMAKE_LFLAGS_RELEASE_WITH_DEBUGINFO

    debug:LIBS += -lgtestd
    !debug:LIBS += -lgtest
}
else {
    exists(/opt/gtest/gtest-1.10.0/lib) {
        LIBS += -L/opt/gtest/gtest-1.10.0/lib
        INCLUDEPATH += /opt/gtest/gtest-1.10.0/include
    }
    LIBS += -lgtest
    LIBS += -lpthread
}

include(../../MEGAcmdServer/MEGAcmdServer.pri)


CONFIG -= c++11
QMAKE_CXXFLAGS-=-std=c++11
CONFIG += c++17
QMAKE_CXXFLAGS+=-std=c++17

#QMAKE_CXXFLAGS += "-fsanitize=address"
#QMAKE_CXXFLAGS_DEBUG += "-fsanitize=address"
#QMAKE_LFLAGS += "-fsanitize=address"

include(../MEGAcmdTest_common/MEGAcmdTest_common.pri)

INCLUDEPATH += \
$$MEGACMD_BASE_PATH/src

MEGACMD_BASE_PATH_RELATIVE = ../../../../..
MEGACMD_BASE_PATH = $$PWD/$$MEGACMD_BASE_PATH_RELATIVE

SOURCES += \
$$MEGACMD_BASE_PATH/tests/integration/BasicTests.cpp \
$$MEGACMD_BASE_PATH/tests/integration/main.cpp

HEADERS += \
$$MEGACMD_BASE_PATH/tests/integration/MegaCmdTestingTools.h
