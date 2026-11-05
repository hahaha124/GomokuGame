QT       += core gui widgets
TARGET    = GomokuGame
TEMPLATE  = app
CONFIG    += c++17

SOURCES += \
    CBoardView.cpp \
    CMainWidget.cpp \
    main.cpp \
    gomoku.cpp
    
HEADERS += \
    CBoardView.h \
    CMainWidget.h \
    gomoku.h

RESOURCES += \
    img.qrc
