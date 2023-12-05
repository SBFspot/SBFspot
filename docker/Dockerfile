# buildtime container
FROM alpine:latest AS builder

ARG sbfspot_home=/usr/local/bin/sbfspot.3

# for SBFspot
RUN apk --no-cache add git make g++ boost-dev bluez-dev sqlite-dev mariadb-dev

WORKDIR /tmp
RUN git clone https://github.com/SBFspot/SBFspot.git

WORKDIR /tmp/SBFspot/SBFspot
RUN sed -i s-\#ifdef\ linux-\#ifdef\ linux\\n\#include\ \<sys\/select.h\>- Ethernet.h
RUN sed -i s-\#ifdef\ linux-\#ifdef\ linux\\n\#include\ \<sys\/select.h\>- bluetooth.h
RUN sed -i s/mariadbclient/mariadb/ makefile

RUN make nosql && make sqlite && make mysql && make mariadb

# install files
RUN make install_nosql && mv /usr/local/bin/sbfspot.3/SBFspot /usr/local/bin/sbfspot.3/SBFspot_nosql \
      && cp /tmp/SBFspot/SBFspot/sqlite/bin/SBFspot /usr/local/bin/sbfspot.3/SBFspot_sqlite \
      && cp /tmp/SBFspot/SBFspot/mysql/bin/SBFspot /usr/local/bin/sbfspot.3/SBFspot_mysql \
      && cp /tmp/SBFspot/SBFspot/mariadb/bin/SBFspot /usr/local/bin/sbfspot.3/SBFspot_mariadb \
      && cp /tmp/SBFspot/SBFspot/*.sql /usr/local/bin/sbfspot.3


# for SBFspotUploadDaemon
RUN apk --no-cache add curl-dev

WORKDIR /tmp/SBFspot/SBFspotUploadDaemon
RUN sed -i s/mariadbclient/mariadb/ makefile

RUN make sqlite && make mysql && make mariadb

#install files
RUN mkdir /etc/sbfspot /var/sbfspot
RUN make install_sqlite && mv $sbfspot_home/SBFspotUploadDaemon $sbfspot_home/SBFspotUploadDaemon_sqlite \
      && cp /tmp/SBFspot/SBFspotUploadDaemon/mysql/bin/SBFspotUploadDaemon $sbfspot_home/SBFspotUploadDaemon_mysql \
      && cp /tmp/SBFspot/SBFspotUploadDaemon/mariadb/bin/SBFspotUploadDaemon $sbfspot_home/SBFspotUploadDaemon_mariadb

# install database tools for copying to new image
RUN apk --no-cache add mariadb-client

##############################################################################################################

# runtime container
FROM alpine:latest

ARG user=sbfspot
ARG group=sbfspot
ARG uid=5000
ARG gid=5000
ARG confdir=/etc/sbfspot
ARG datadir=/var/sbfspot
ARG sbfspot_home=/usr/local/bin/sbfspot.3

ENV SBFSPOT_INTERVAL 600

# install Packages needed for SBFspot usage, tzdata for setting local time, mariadb-common, mariadb-connector-c and ncurses-libs for mysql command, sqlite for sqlite3 command, libcurl for SBFspotUploadDaemon, mosquitto-clients for mosquitto_pub, sed for setConfigValue
RUN apk --no-cache add boost-date_time bluez-libs libstdc++ sqlite-libs tzdata mariadb-common mariadb-connector-c ncurses-libs sqlite libcurl mosquitto-clients sed

# copy SBFspot
COPY --from=builder $sbfspot_home $sbfspot_home
COPY ./start.sh /

# copy tools
COPY --from=builder /usr/bin/mysql /usr/bin/mysql

RUN mkdir $confdir $datadir \
      && chown $uid $confdir $datadir

RUN addgroup -g $gid -S $group && adduser -S -D -G $group -H -u $uid $user

RUN chmod g+w /var/log \
      && chgrp $group /var/log
      
RUN chmod u+x /start.sh \
      && chown $user:$group /start.sh
      
USER $user

ENTRYPOINT ["/start.sh"]
