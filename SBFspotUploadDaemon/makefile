############################################################
# Makefile for SBFspotUploadDaemon
#
# Compilation: 
#	make sqlite|mysql|mariadb
#
# Installation:
#	sudo make install_sqlite|install_mysql|install_mariadb
#
############################################################

CC = gcc
CXX = g++
AR = ar
LD = g++

INSTALLDIR  = /usr/local/bin/sbfspot.3/

APPNAME = SBFspotUploadDaemon

SRC_COMMON := ../SBFspotUploadCommon
SRC_SBFSPOT:= ../SBFspot
SRC_NOOPT  := $(SRC_COMMON)/PVOutput_x.cpp
SRC_MAIN   := main.cpp $(SRC_COMMON)/Configuration.cpp $(SRC_COMMON)/CommonServiceCode.cpp $(SRC_COMMON)/PVOutput.cpp
SRC_SQLITE := $(SRC_MAIN) $(SRC_SBFSPOT)/db_SQLite.cpp
SRC_MYSQL  := $(SRC_MAIN) $(SRC_SBFSPOT)/db_MySQL.cpp
SRC_MARIADB:= $(SRC_MYSQL)

CFLAGS     := -c -Wall -Wno-unused-local-typedefs -std=c++11
CFLAGS_OPT := -O2
INCDIR     := ../SBFspot
LIBDIR     :=
LIBS       := pthread curl
LDFLAGS    := -s

# Default Target = Install SQLite
ifeq ($(MAKECMDGOALS),)
	MAKECMDGOALS = install_sqlite
endif

ifeq ($(MAKECMDGOALS),sqlite)
BINDIR     := sqlite/bin/
OBJDIR     := sqlite/obj/
OBJECTS    := $(SRC_SQLITE:%.cpp=$(OBJDIR)%.o)
CFLAGS     := $(CFLAGS) -DUSE_SQLITE
LIBS       := $(LIBS) sqlite3
else ifeq ($(MAKECMDGOALS),mysql)
BINDIR     := mysql/bin/
OBJDIR     := mysql/obj/
OBJECTS    := $(SRC_MYSQL:%.cpp=$(OBJDIR)%.o)
CFLAGS     := $(CFLAGS) -DUSE_MYSQL
LIBS       := $(LIBS) mysqlclient
else ifeq ($(MAKECMDGOALS),mariadb)
BINDIR     := mariadb/bin/
OBJDIR     := mariadb/obj/
OBJECTS    := $(SRC_MARIADB:%.cpp=$(OBJDIR)%.o)
CFLAGS     := $(CFLAGS) -DUSE_MYSQL
LIBS       := $(LIBS) mariadbclient
else ifeq ($(MAKECMDGOALS),install_sqlite)
BINDIR     := sqlite/bin/
else ifeq ($(MAKECMDGOALS),install_mysql)
BINDIR     := mysql/bin/
else ifeq ($(MAKECMDGOALS),install_mariadb)
BINDIR     := mariadb/bin/
endif

TARGET     := $(BINDIR)$(APPNAME)

CMD_RMDIR = rm -rf
CMD_MKDIR = || mkdir -p
CMD_TESTDIR = test -d
CMD_COPY = cp

sqlite: init_build build_target

mysql: init_build build_target

mariadb: init_build build_target

install_sqlite: init_install install

install_mysql: init_install install

install_mariadb: init_install install

init_build:
	$(CMD_TESTDIR) $(OBJDIR) $(CMD_MKDIR) $(OBJDIR)
	$(CMD_TESTDIR) $(OBJDIR)/$(SRC_COMMON) $(CMD_MKDIR) $(OBJDIR)/$(SRC_COMMON)
	$(CMD_TESTDIR) $(OBJDIR)/$(SRC_SBFSPOT) $(CMD_MKDIR) $(OBJDIR)/$(SRC_SBFSPOT)
	$(CMD_TESTDIR) $(BINDIR) $(CMD_MKDIR) $(BINDIR)

init_install:
	$(CMD_TESTDIR) $(INSTALLDIR) $(CMD_MKDIR) $(INSTALLDIR)
	
build_target: $(TARGET)

install:
	$(CMD_COPY) $(TARGET) $(INSTALLDIR)
	$(CMD_COPY) SBFspotUpload.default.cfg $(INSTALLDIR)

OBJECTS_NOOPT := $(SRC_NOOPT:%.cpp=$(OBJDIR)%.o)

$(OBJECTS): EXTRA_FLAGS := $(CFLAGS_OPT)

$(OBJDIR)%.o: %.cpp
	$(CXX) $^ $(CFLAGS) $(EXTRA_FLAGS) -o $@ -I$(INCDIR)

$(TARGET): $(OBJECTS_NOOPT) $(OBJECTS)
	$(LD) $^ $(LDFLAGS) -o $@ -Wl,-Bdynamic $(addprefix -l,$(LIBS)) $(addprefix -L,$(LIBDIR))

cleanall:
	$(CMD_RMDIR) sqlite
	$(CMD_RMDIR) mysql
	$(CMD_RMDIR) mariadb

clean: cleanall

.PHONY: sqlite mysql mariadb install_sqlite install_mysql install_mariadb cleanall clean