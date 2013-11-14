TARGETTYPE=DRIVER

!IF !DEFINED(TARGETNAME)
TARGETNAME=umkcf
!ENDIF

!IF !DEFINED(TARGETPATH)
TARGETPATH=..\bin
!ENDIF

INCLUDES=$(DDK_INC_PATH);..\include;..\..\include
LIBS=%BUILD%\lib

SOURCES= \
    ..\main.c \
    ..\devctrl.c \
    ..\resource.rc
