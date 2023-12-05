# sbfspot

This Docker Image provides SBFspot, an open source project located at github ([SBFspot on github](https://github.com/SBFspot/SBFspot)).
It includes:

* SBFspot is used to read power generation data from SMA® solar/battery inverters and store it in a database, csv files or send it to an MQTT Broker.
* SBFspotUploadDaemon can read the power data (only if stored in a mysql/mariadb/sqlite database) and transmit it to ([PVoutput.org](https://pvoutput.org/)).

## Prerequisites

To run SBFspot, you must provide a configuration file (SBFspot.cfg) and map it in the container. Please also make a backup of your config as the container edits the file.
To run SBFspotUploadDaemon, you must provide a configuration file (SBFspotUpload.cfg) and map it in the container.

## Caveats

If your Inverters communicate with Bluetooth, you have to start the SBFspot container in the host network (Option --network host, see usage examples below). Thereby every device including Bluetooth devices are mapped to the container.
The bad about this is, that every host-device is also accessible in the container. If someone knows a workaround or besser solution for this, then please let me know.

# Volumes
* /etc/sbfspot => directory for your configuration files
* /var/sbfspot => data directory (for storing csv files or sqlite database file)

The mapped host volumes should be read- and writeable for user with ID 5000 or group with ID 5000, the config files also.

# Environment variables
## General
### ENABLE_SBFSPOT
* **0** => disable SBFspot
* **1** => enable SBFspot to collect power production data from your inverter(s)

### ENABLE_SBFSPOT_UPLOAD
* **0** => disable SBFspotUploadDaemon
* **1** => enable SBFspotUploadDaemon to upload your production data to PVoutput

### SBFSPOT_INTERVAL
* **seconds** => define an interval at which SBFspot should poll your inverter(s) - default 600

### TZ
* **timezone** e.g. Europe/Berlin => Provide your local timezone

List of all time zones can be found on [wikipedias tz database site](https://en.wikipedia.org/wiki/List_of_tz_database_time_zones), column "TZ database name". 

### DB_STORAGE
If you want to upload your production data to PVoutput, you have to store them in a database. Otherwise, it is optional. Read the paragraph "Database initialization" to setup your DB.

* **sqlite** => use a sqlite database
* **mysql** => use a mysql database 
* **mariadb** => use a mariadb database

### CSV_STORAGE
If you want to store your production data in CSV files, chose this option. You can additionally store your data in a Database and/or publish them through MQTT.

* **0** => do not store csv files
* **1** => store production data in csv files

### MQTT_ENABLE
If you want to publish your production data to an MQTT Broker, chose this option. You can additionally store your data in a Database and/or CSV files.

* **0** => do not publish data
* **1** => publish production data to an MQTT Broker

(If you want to use MQTT, setup the MQTT Options in the SBFspot.cfg file.)

## SBFspot options
The following Options can be used directly as environment variables.
### QUIET
* **1** => no output from SBFspot

### FINQ or alternatively FORCE
* **1** => Inquire the inverter even at night

### SBFSPOT_ARGS
* **List of Args** => if you don't find the needed Option as an Environment Variable, you can pass the Args directly to the SBFSPOT_ARGS Var.
Example: ```SBFSPOT_ARGS="-finq -settime"``` (Separate multiple Args with space)

    Following a list of useable SBFspots Options
    
    ```
    -d#                 Set debug level: 0-5 (0=none, default=2)
    -v#                 Set verbose output level: 0-5 (0=none, default=2)
    -ad#                Set #days for archived daydata: 0-300
                        0=disabled, 1=today (default), ...
    -am#                Set #months for archived monthdata: 0-300
                        0=disabled, 1=current month (default), ...
    -ae#                Set #months for archived events: 0-300
                        0=disabled, 1=current month (default), ...
    -finq               Force Inquiry (Inquire inverter also during the night)
    -q                  Quiet (No output)
    -nocsv              Disables CSV export (Overrules CSV_Export in config)
    -nosql              Disables SQL export
    -sp0                Disables Spot.csv export
    -installer          Login as installer
    -password:xxxx      Installer password
    -loadlive           Use predefined settings for manual upload to pvoutput.org
    -startdate:YYYYMMDD Set start date for historic data retrieval
    -settime            Sync inverter time with host time
    ```
 
## Database initialization
### INIT_DB
* **0** => normal Operation (poll Inverter and / or upload Data to PVoutput)
* **1** => initialize Database

If you choose ```sqlite``` in the ```DB_STORAGE``` variable, a new database file under /var/sbfspot will be created and set up.

If you choose ```mysql``` or ```mariadb```, a connection to the DB Server configured in SBFspot.cfg will be opened and a SBFspot database and user created. (You have to setup your user and DB Connection in ```SBFspot.cfg```.)

### DB_ROOT_USER
* **username** => provide root username to your mysql/mariadb database (or the username of an user with grants for creating a new database and adding a new user)

### DB_ROOT_PW
* **password** => root password

After the Database is set up, please delete the three database initialization variables. For security reasons, normal operation is prohibited, if administrative accounts are provided. 


# Usage
Examples for using the container:
Initialize a new mysql Database

```
docker run -e "DB_STORAGE=mysql" -e "INIT_DB=1" -e "DB_ROOT_USER=root" -e "DB_ROOT_PW=secret" 
   -v /path/to/your/config/dir/on/host:/etc/sbfspot -v /path/to/your/data/dir/on/host:/var/sbfspot nakla/sbfspot:latest
```

Start only SBFspot and store inverters data in a mariadb Database configured in SBFspot.conf

```
docker run --network host -e "DB_STORAGE=mariadb" -e "ENABLE_SBFSPOT=1" -e "TZ=Europe/Berlin" 
   -v /path/to/your/config/dir/on/host:/etc/sbfspot -v /path/to/your/data/dir/on/host:/var/sbfspot nakla/sbfspot:latest
```

Start only SBFspot and store inverters data in csv files on the host in the directory /path/to/your/data/dir/on/host

```
docker run --network host -e "CSV_STORAGE=1" -e "ENABLE_SBFSPOT=1" -e "TZ=Europe/Berlin" 
   -v /path/to/your/config/dir/on/host:/etc/sbfspot -v /path/to/your/data/dir/on/host:/var/sbfspot nakla/sbfspot:latest
```

Start SBFspot, store inverters data in a mysql Database and upload inverters Data to pvoutput.org

```
docker run --network host -e "DB_STORAGE=mysql" -e "ENABLE_SBFSPOT=1" -e "ENABLE_SBFSPOT_UPLOAD=1" 
   -v /path/to/your/config/dir/on/host:/etc/sbfspot -v /path/to/your/data/dir/on/host:/var/sbfspot nakla/sbfspot:latest

```

You can also use the following docker-compose.yaml file to start your container.

```
version: '3'

services:
    sbfspot:
        image: nakla/sbfspot:latest
        network_mode: host
        volumes:
            - ~/sbfspot/etc:/etc/sbfspot
            - ~/sbfspot/data:/var/sbfspot
        environment:
            TZ: Europe/Berlin
            ENABLE_SBFSPOT: 1
            SBFSPOT_INTERVAL: 600
            ENABLE_SBFSPOT_UPLOAD: 0
            DB_STORAGE: sqlite
            CSV_STORAGE: 1
            MQTT_ENABLE: 1
            QUIET: 0
            SBFSPOT_ARGS: -d0 -v2
            INIT_DB: 0
        restart: always

```

# License

[Attribution - NonCommercial - ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)](https://creativecommons.org/licenses/by-nc-sa/3.0/legalcode)

In short, you are free:

* to Share => to copy, distribute and transmit the work
* to Remix => to adapt the work Under the following conditions:
* Attribution: You must attribute the work in the manner specified by the author or Licensor (but not in any way that suggests that they endorse you or your use of the work).
* Noncommercial: You may not use this work for commercial purposes.
* Share Alike: If you alter, transform, or build upon this work, you may distribute the resulting work only under the same or similar license to this one.

# Disclaimer

A user of SBFspot software acknowledges that he or she is receiving this software on an "as is" basis and the user is not relying on the accuracy or functionality of the software for any purpose. The user further acknowledges that any use of this software will be at his own risk and the copyright owner accepts no responsibility whatsoever arising from the use or application of the software.

SMA, Speedwire are registered trademarks of [SMA Solar Technology AG](http://www.sma.de/en/company/about-sma.html)
