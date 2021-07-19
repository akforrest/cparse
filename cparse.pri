
CPARSE_DIR = $$PWD

SOURCES += \
    $$PWD/shunting-yard.cpp \
    $$PWD/packtoken.cpp \
    $$PWD/functions.cpp \
    $$PWD/containers.cpp \
    $$PWD/calculator.cpp

INCLUDEPATH += CPARSE_DIR
HEADERS += \
    $$PWD/config.h \
    $$PWD/cparse.h \
    $$PWD/operation.h \
    $$PWD/shunting-yard.h \
    $$PWD/packtoken.h \
    $$PWD/functions.h \
    $$PWD/containers.h \
    $$PWD/calculator.h \
    $$PWD/tokenbase.h \
    $$PWD/cparse-types.h \
    #$$PWD/shunting-yard-exceptions.h \
    #$$PWD/builtin-features.inc \
    $$PWD/builtin-features\functions.h \
    $$PWD/builtin-features\operations.h \
    $$PWD/builtin-features\reservedWords.h \
    $$CPARSE_DIR/builtin-features\typeSpecificFunctions.h \
    $$PWD/tokenhelpers.h
