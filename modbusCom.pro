TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c \
    libmodbus/modbus-tcp.c \
    libmodbus/modbus-rtu.c \
    libmodbus/modbus-data.c \
    libmodbus/modbus.c

HEADERS += \
    libmodbus/modbus-version.h \
    libmodbus/modbus-tcp-private.h \
    libmodbus/modbus-tcp.h \
    libmodbus/modbus-rtu-private.h \
    libmodbus/modbus-rtu.h \
    libmodbus/modbus-private.h \
    libmodbus/modbus.h \
    libmodbus/config.h

INCLUDEPATH += libmodbus

LIBS += -lrt
