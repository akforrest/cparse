
CPARSE_DIR = $$PWD

SOURCES += \
    $$CPARSE_DIR/shunting-yard.cpp \
    $$CPARSE_DIR/packtoken.cpp \
    $$CPARSE_DIR/functions.cpp \
    $$CPARSE_DIR/containers.cpp

INCLUDEPATH += CPARSE_DIR
HEADERS += \
    $$CPARSE_DIR/shunting-yard.h \
    #$$CPARSE_DIR/functions.h \
    #$$CPARSE_DIR/containers.h \
    #$$CPARSE_DIR/packtoken.h \
    #$$CPARSE_DIR/shunting-yard-exceptions.h \
    #$$CPARSE_DIR/builtin-features.inc \
    #$$CPARSE_DIR/builtin-features\functions.inc \
    #$$CPARSE_DIR/builtin-features\operations.inc \
    #$$CPARSE_DIR/builtin-features\reservedWords.inc \
    #$$CPARSE_DIR/builtin-features\typeSpecificFunctions.inc
