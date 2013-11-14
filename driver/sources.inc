TARGETTYPE=DRIVER

!IF !DEFINED(TARGETNAME)
TARGETNAME=umkcf
!ENDIF

!IF !DEFINED(TARGETPATH)
TARGETPATH=..\bin
!ENDIF

INCLUDES=$(DDK_INC_PATH);..\include;..\..\include;..\..\include\sys
LIBS=%BUILD%\lib

SOURCES= \
    ..\main.c \
	..\client.c \
    ..\devctrl.c \
	..\pscall.c \
    ..\resource.rc
