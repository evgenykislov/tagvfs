TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
  tag_dir.c \
  tag_file.c \
  tag_fs.c \
  tag_inode.c \
  tag_module.c \
  tag_storage.c

HEADERS += \
  tag_dir.h \
  tag_file.h \
  tag_fs.h \
  tag_inode.h \
  tag_storage.h

DISTFILES += \
  Makefile
