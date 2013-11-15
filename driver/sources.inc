TARGETTYPE=DRIVER

!IF !DEFINED(TARGETNAME)
TARGETNAME=umkcf
!ENDIF

!IF !DEFINED(TARGETPATH)
TARGETPATH=..\bin
!ENDIF

INCLUDES=$(DDK_INC_PATH);..\include;..\..\include;..\..\include\sys
LIBS=%BUILD%\lib

MSC_WARNING_LEVEL=/WX
LINKER_FLAGS=/INTEGRITYCHECK

SOURCES= \
    ..\main.c \
	..\client.c \
    ..\devctrl.c \
	..\filter.c \
	..\pscall.c \
    ..\resource.rc
