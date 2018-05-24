#drop database if exists SBFspot;

CREATE Database SBFspot;

USE SBFspot;

CREATE Table Config (
	`Key` varchar(32),
	`Value` varchar(200),
	PRIMARY KEY (`Key`)
);

INSERT INTO Config VALUES('SchemaVersion','1');

CREATE Table Inverters (
	Serial int(4) NOT NULL,
	Name varchar(32),
	Type varchar(32),
	SW_Version varchar(32),
	TimeStamp int(4),
	TotalPac int(4),
	EToday int(8),
	ETotal int(8),
	OperatingTime double,
	FeedInTime double,
	Status varchar(10),
	GridRelay varchar(10),
	Temperature float,
	PRIMARY KEY (Serial)
);

CREATE View vwInverters AS
	Select Serial,
	Name,Type,SW_Version,
	From_UnixTime(TimeStamp) AS TimeStamp,
	TotalPac,
	EToday,ETotal,
	OperatingTime,FeedInTime,
	Status,GridRelay,
	Temperature
	FROM Inverters;

CREATE Table SpotData (
	TimeStamp int(4) NOT NULL,
	Serial int(4) NOT NULL,
	Pdc1 int(4), Pdc2 int(4),
	Idc1 float, Idc2 float,
	Udc1 float, Udc2 float,
	Pac1 int(4), Pac2 int(4), Pac3 int(4),
	Iac1 float, Iac2 float, Iac3 float,
	Uac1 float, Uac2 float, Uac3 float,
	EToday int(8), ETotal int(8),
	Frequency float,
	OperatingTime double,
	FeedInTime double,
	BT_Signal float,
	Status varchar(10),
	GridRelay varchar(10),
	Temperature float,
	PRIMARY KEY (TimeStamp, Serial)
);

-- Fix 02-MAY-2016 See Issue 150
CREATE View vwSpotData AS
    Select From_UnixTime(Dat.TimeStamp) AS TimeStamp,
    From_UnixTime(CASE WHEN (Dat.TimeStamp % 300) < 150
    THEN Dat.TimeStamp - (Dat.TimeStamp % 300)
    ELSE Dat.TimeStamp - (Dat.TimeStamp % 300) + 300
    END) AS Nearest5min,
    Inv.Name,
    Inv.Type,
    Dat.Serial,
    Pdc1, Pdc2,
    Idc1, Idc2,
    Udc1, Udc2,
    Pac1, Pac2, Pac3,
    Iac1, Iac2, Iac3,
    Uac1, Uac2, Uac3,
    Pdc1+Pdc2 AS PdcTot,
    Pac1+Pac2+Pac3 AS PacTot,
    CASE WHEN Pdc1+Pdc2 = 0 THEN
        0
    ELSE
        CASE WHEN Pdc1+Pdc2>Pac1+Pac2+Pac3 THEN
            ROUND((Pac1+Pac2+Pac3)/(Pdc1+Pdc2)*100,1)
        ELSE
            100.0
        END
    END AS Efficiency,
    Dat.EToday,
    Dat.ETotal,
    Frequency,
    Dat.OperatingTime,
    Dat.FeedInTime,
    ROUND(BT_Signal,1) AS BT_Signal,
    Dat.Status,
    Dat.GridRelay,
    ROUND(Dat.Temperature,1) AS Temperature
    FROM SpotData Dat
INNER JOIN Inverters Inv ON Dat.Serial=Inv.Serial;

CREATE Table DayData (
	TimeStamp int(4) NOT NULL,
	Serial int(4) NOT NULL,
	TotalYield int(8),
	Power int(8),
	PVoutput int(1),
	PRIMARY KEY (TimeStamp, Serial)
);

CREATE View vwDayData AS
	select From_UnixTime(Dat.TimeStamp) AS TimeStamp,
	Inv.Name, Inv.Type, Dat.Serial,
	TotalYield,
	Power,
	PVOutput FROM DayData Dat
INNER JOIN Inverters Inv ON Dat.Serial=Inv.Serial
ORDER BY Dat.Timestamp Desc;

CREATE Table MonthData (
	TimeStamp int(4) NOT NULL,
	Serial int(4) NOT NULL,
	TotalYield int(8),
	DayYield int(8),
	PRIMARY KEY (TimeStamp, Serial)
);

CREATE View vwMonthData AS
	select convert_tz(from_unixtime(Dat.TimeStamp), 'SYSTEM', '+00:00') AS TimeStamp,
	Inv.Name, Inv.Type, Dat.Serial,
	TotalYield, DayYield FROM MonthData Dat
INNER JOIN Inverters Inv ON Dat.Serial=Inv.Serial
ORDER BY Dat.Timestamp Desc;

CREATE Table EventData (
	EntryID int(4),
	TimeStamp int(4) NOT NULL,
	Serial int(4) NOT NULL,
	SusyID int(2),
	EventCode int(4),
	EventType varchar(32),
	Category varchar(32),
	EventGroup varchar(32),
	Tag varchar(200),
	OldValue varchar(32),
	NewValue varchar(32),
	UserGroup varchar(10),
	PRIMARY KEY (Serial, EntryID)
);

CREATE View vwEventData AS
	select From_UnixTime(Dat.TimeStamp) AS TimeStamp,
	Inv.Name, Inv.Type, Dat.Serial,
	SusyID, EntryID,
	EventCode, EventType,
	Category, EventGroup, Tag,
	OldValue, NewValue,
	UserGroup FROM EventData Dat
INNER JOIN Inverters Inv ON Dat.Serial=Inv.Serial
ORDER BY EntryID Desc;

CREATE Table Consumption (
	TimeStamp int(4) NOT NULL,
	EnergyUsed int(4),
	PowerUsed int(4),
	PRIMARY KEY (TimeStamp)
);

CREATE VIEW vwConsumption AS
	SELECT From_UnixTime(TimeStamp) As Timestamp,
	From_UnixTime(CASE WHEN (TimeStamp % 300) < 150
	THEN TimeStamp - (TimeStamp % 300)
	ELSE TimeStamp - (TimeStamp % 300) + 300
	END) AS Nearest5min,
	EnergyUsed,
	PowerUsed
	FROM Consumption;

-- Fix 02-MAY-2016 See Issue 150
CREATE VIEW vwAvgConsumption AS
    SELECT Nearest5min,
    cast(avg(EnergyUsed) As decimal(9)) As EnergyUsed,
    cast(avg(PowerUsed) As decimal(9)) As PowerUsed
    FROM vwConsumption
    GROUP BY Nearest5Min;

CREATE VIEW vwAvgSpotData AS
       SELECT nearest5min,
              serial,
              cast(avg(Pdc1) as decimal(9)) AS Pdc1,
              cast(avg(Pdc2) as decimal(9)) AS Pdc2,
              cast(avg(Idc1) as decimal(9,3)) AS Idc1,
              cast(avg(Idc2) as decimal(9,3)) AS Idc2,
              cast(avg(Udc1) as decimal(9,2)) AS Udc1,
              cast(avg(Udc2) as decimal(9,2)) AS Udc2,
              cast(avg(Pac1) as decimal(9)) AS Pac1,
              cast(avg(Pac2) as decimal(9)) AS Pac2,
              cast(avg(Pac3) as decimal(9)) AS Pac3,
              cast(avg(Iac1) as decimal(9,3)) AS Iac1,
              cast(avg(Iac2) as decimal(9,3)) AS Iac2,
              cast(avg(Iac3) as decimal(9,3)) AS Iac3,
              cast(avg(Uac1) as decimal(9,2)) AS Uac1,
              cast(avg(Uac2) as decimal(9,2)) AS Uac2,
              cast(avg(Uac3) as decimal(9,2)) AS Uac3,
              cast(avg(Temperature) as decimal(9,2)) AS Temperature
        FROM vwSpotData
        GROUP BY serial, nearest5min;

CREATE VIEW vwPvoData AS
       SELECT dd.Timestamp,
              dd.Name,
              dd.Type,
              dd.Serial,
              dd.TotalYield AS V1,
              dd.Power AS V2,
              cons.EnergyUsed AS V3,
              cons.PowerUsed AS V4,
              spot.Temperature AS V5,
              spot.Uac1 AS V6,
              NULL AS V7,
              NULL AS V8,
              NULL AS V9,
              NULL AS V10,
              NULL AS V11,
              NULL AS V12,
              dd.PVoutput
         FROM vwDayData AS dd
              LEFT JOIN vwAvgSpotData AS spot
                     ON dd.Serial = spot.Serial AND dd.Timestamp = spot.Nearest5min
              LEFT JOIN vwAvgConsumption AS cons
                     ON dd.Timestamp = cons.Nearest5min
        ORDER BY dd.Timestamp DESC;

-- Fix 09-JAN-2017 See Issue 54: SQL Support for battery inverters
CREATE TABLE SpotDataX (
    `TimeStamp` INTEGER (4) NOT NULL,
    `Serial`    INTEGER (4) NOT NULL,
    `Key`       INTEGER (4) NOT NULL,
    `Value`     INTEGER (4),
    PRIMARY KEY (
        `TimeStamp` ASC,
        `Serial` ASC,
        `Key`
    )
);

DROP VIEW IF EXISTS vwBatteryData;

CREATE VIEW vwBatteryData AS
    SELECT FROM_UNIXTIME(sdx.`TimeStamp` - (sdx.`TimeStamp` % 300)) AS `5min`,
           sdx.`Serial`,
           inv.`Name`,
           AVG(CASE WHEN `Key` = 10586 THEN `Value` END) AS ChaStatus,
           AVG(CASE WHEN `Key` = 18779 THEN CAST(`Value` AS DECIMAL(10,1)) / 10 END) AS Temperature,
           AVG(CASE WHEN `Key` = 18781 THEN CAST(`Value` AS DECIMAL(10,3)) / 1000 END) AS ChaCurrent,
           AVG(CASE WHEN `Key` = 18780 THEN CAST(`Value` AS DECIMAL(10,2)) / 100 END) AS ChaVoltage,
           AVG(CASE WHEN `Key` = 17974 THEN `Value` END) AS GridMsTotWOut,
           AVG(CASE WHEN `Key` = 17975 THEN `Value` END) AS GridMsTotWIn
      FROM SpotDataX AS sdx
           INNER JOIN
           Inverters AS inv ON sdx.Serial = inv.`Serial`
     GROUP BY `5min`;
