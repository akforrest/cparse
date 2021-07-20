
SOURCES += \
    $$PWD/config.cpp \
    $$PWD/packtoken.cpp \
    $$PWD/functions.cpp \
    $$PWD/containers.cpp \
    $$PWD/calculator.cpp \
    $$PWD/reftoken.cpp \
    $$PWD/rpnbuilder.cpp

INCLUDEPATH += $$PWD
HEADERS += \
    $$PWD/config.h \
    $$PWD/cparse-test.h \
    $$PWD/cparse.h \
    $$PWD/operation.h \
    $$PWD/packtoken.h \
    $$PWD/functions.h \
    $$PWD/containers.h \
    $$PWD/calculator.h \
    $$PWD/reftoken.h \
    $$PWD/rpnbuilder.h \
    $$PWD/builtin-features\functions.h \
    $$PWD/builtin-features\operations.h \
    $$PWD/builtin-features\reservedwords.h \
    $$PWD/builtin-features\typespecificfunctions.h \
    $$PWD/token.h \
    $$PWD/tokenhelpers.h \
    $$PWD/tokentype.h

## TEST BITS
CONFIG += cparseUnitTests
cparseUnitTests {
    CONFIG += no_testcase_installs
    QT += testlib
    SOURCES += $$PWD/cparse-test.cpp
    HEADERS += $$PWD/cparse-test.h
}
