TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
  common.c \
  tag_allfiles_dir.c \
  tag_dir.c \
  tag_file.c \
  tag_fs.c \
  tag_inode.c \
  tag_module.c \
  tag_storage.c \
  tag_tag_mask.c

HEADERS += \
  common.h \
  tag_allfiles_dir.h \
  tag_dir.h \
  tag_file.h \
  tag_fs.h \
  tag_inode.h \
  tag_storage.h \
  tag_tag_mask.h

DISTFILES += \
  Makefile
