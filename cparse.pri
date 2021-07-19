
CPARSE_DIR = $$PWD

SOURCES += \
    $$PWD/packtoken.cpp \
    $$PWD/functions.cpp \
    $$PWD/containers.cpp \
    $$PWD/calculator.cpp \
    $$PWD/shuntingyard.cpp

INCLUDEPATH += CPARSE_DIR
HEADERS += \
    $$PWD/config.h \
    $$PWD/cparse.h \
    $$PWD/operation.h \
    $$PWD/packtoken.h \
    $$PWD/functions.h \
    $$PWD/containers.h \
    $$PWD/calculator.h \
    $$PWD/shuntingyard.h \
    $$PWD/shuntingyardexceptions.h \
    #$$PWD/shunting-yard-exceptions.h \
    #$$PWD/builtin-features.inc \
    $$PWD/builtin-features\functions.h \
    $$PWD/builtin-features\operations.h \
    $$PWD/builtin-features\reservedwords.h \
    $$CPARSE_DIR/builtin-features\typespecificfunctions.h \
    $$PWD/token.h \
    $$PWD/tokenhelpers.h \
    $$PWD/tokentype.h
