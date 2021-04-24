###########################################################################
# Makefile for SBFspot V3.x
#
# Compilation: 
#	make nosql|sqlite|mysql|mariadb
#
# Installation:
#	sudo make install_nosql|install_sqlite|install_mysql|install_mariadb
#
###########################################################################

CC = gcc
CXX = g++
AR = ar
LD = g++

APPNAME = SBFspot
INSTALLDIR = /usr/local/bin/sbfspot.3/

SRC_NOSQL  := boost_ext.cpp main.cpp misc.cpp sunrise_sunset.cpp SBFNet.cpp CSVexport.cpp Ethernet.cpp EventData.cpp Inverter.cpp ArchData.cpp SBFspot.cpp TagDefs.cpp Bluetooth.cpp mqtt.cpp
SRC_SQLITE := $(SRC_NOSQL) db_SQLite.cpp db_SQLite_Export.cpp
SRC_MYSQL  := $(SRC_NOSQL) db_MySQL.cpp db_MySQL_Export.cpp
SRC_MARIADB:= $(SRC_MYSQL)

CFLAGS     := -c -Wall -O2 -Wno-unused-local-typedefs -Wno-psabi -std=c++11
INCDIR     :=
LIBDIR     :=
LIBS       := pthread bluetooth boost_date_time boost_system
LDFLAGS    := -s

# Default Target = Install SQLite
ifeq ($(MAKECMDGOALS),)
	MAKECMDGOALS = install_sqlite
endif

ifeq ($(MAKECMDGOALS),nosql)
BINDIR     := nosql/bin/
OBJDIR     := nosql/obj/
OBJECTS    := $(SRC_NOSQL:%.cpp=$(OBJDIR)%.o)
else ifeq ($(MAKECMDGOALS),sqlite)
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
else ifeq ($(MAKECMDGOALS),install_nosql)
BINDIR     := nosql/bin/
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

nosql: init_build build_target

sqlite: init_build build_target

mysql: init_build build_target

mariadb: init_build build_target

install_nosql: init_install install

install_sqlite: init_install install

install_mysql: init_install install

install_mariadb: init_install install

init_build:
	$(CMD_TESTDIR) $(OBJDIR) $(CMD_MKDIR) $(OBJDIR)
	$(CMD_TESTDIR) $(BINDIR) $(CMD_MKDIR) $(BINDIR)
	
init_install:
	$(CMD_TESTDIR) $(INSTALLDIR) $(CMD_MKDIR) $(INSTALLDIR)

build_target: $(TARGET)

install:
	$(CMD_COPY) $(TARGET) $(INSTALLDIR)
	$(CMD_COPY) TagList*.txt $(INSTALLDIR)
	$(CMD_COPY) date_time_zonespec.csv $(INSTALLDIR)
	$(CMD_COPY) SBFspot.cfg $(INSTALLDIR)SBFspot.default.cfg
ifeq ($(MAKECMDGOALS),install_sqlite)
	$(CMD_COPY) *SQLite*.sql $(INSTALLDIR)
endif
ifeq ($(MAKECMDGOALS),install_mysql)
	$(CMD_COPY) *MySQL*.sql $(INSTALLDIR)
endif
ifeq ($(MAKECMDGOALS),install_mariadb)
	$(CMD_COPY) *MySQL*.sql $(INSTALLDIR)
endif
	
$(OBJDIR)%.o: %.cpp
	$(CXX) $^ $(CFLAGS) -o $@ $(addprefix -I,$(INCDIR))

$(TARGET): $(OBJECTS)
	$(LD) $^ $(LDFLAGS) -o $@ -Wl,-Bdynamic $(addprefix -l,$(LIBS)) $(addprefix -L,$(LIBDIR))

cleanall:
	$(CMD_RMDIR) nosql
	$(CMD_RMDIR) sqlite
	$(CMD_RMDIR) mysql
	$(CMD_RMDIR) mariadb

clean: cleanall

.PHONY: nosql sqlite mysql mariadb install_nosql install_sqlite install_mysql install_mariadb cleanall clean
