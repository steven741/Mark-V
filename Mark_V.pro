APP_NAME = Mark_V

LIBS += -lbb -lbbdevice -lbbsystem -lbbplatform -lbbdata -lbbcascadespickers -lscreen -lasound
CONFIG += qt warn_on cascades10

QMAKE_LFLAGS += -fuse-ld=bfd
QMAKE_CXXFLAGS += -std=c++1y

device {
            QMAKE_CC = qcc -V4.8.3,gcc_ntoarmv7le  
            QMAKE_CXX = qcc -V4.8.3,gcc_ntoarmv7le 
            QMAKE_LINK = qcc -V4.8.3,gcc_ntoarmv7le
}
simulator {
            QMAKE_CC = qcc -V4.8.3,gcc_ntox86  
            QMAKE_CXX = qcc -V4.8.3,gcc_ntox86  
            QMAKE_LINK = qcc -V4.8.3,gcc_ntox86
}

include(config.pri)
