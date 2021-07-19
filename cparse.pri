
CPARSE_DIR = $$PWD

SOURCES += \
    $$PWD/packtoken.cpp \
    $$PWD/functions.cpp \
    $$PWD/containers.cpp \
    $$PWD/calculator.cpp \
    $$PWD/reftoken.cpp \
    $$PWD/rpnbuilder.cpp

INCLUDEPATH += CPARSE_DIR
HEADERS += \
    $$PWD/config.h \
    $$PWD/cparse-test.h \
    $$PWD/cparse.h \
    $$PWD/exceptions.h \
    $$PWD/operation.h \
    $$PWD/packtoken.h \
    $$PWD/functions.h \
    $$PWD/containers.h \
    $$PWD/calculator.h \
    $$PWD/reftoken.h \
    $$PWD/rpnbuilder.h \
    #$$PWD/shunting-yard-exceptions.h \
    #$$PWD/builtin-features.inc \
    $$PWD/builtin-features\functions.h \
    $$PWD/builtin-features\operations.h \
    $$PWD/builtin-features\reservedwords.h \
    $$CPARSE_DIR/builtin-features\typespecificfunctions.h \
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
