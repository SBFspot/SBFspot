CREATE Table SpotDataBackup (
        TimeStamp datetime NOT NULL,
        Serial int(4) NOT NULL,
        Pdc1 int, Pdc2 int,
        Idc1 float, Idc2 float,
        Udc1 float, Udc2 float,
        Pac1 int, Pac2 int, Pac3 int,
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

INSERT INTO SpotDataBackup select * from SpotData;

DROP TABLE SpotData;

CREATE Table SpotData (
        TimeStamp datetime NOT NULL,
        Serial int(4) NOT NULL,
        Pdc1 int, Pdc2 int, Pdc3 int,
        Idc1 float, Idc2 float, Idc3 float,
        Udc1 float, Udc2 float, Udc3 float,
        Pac1 int, Pac2 int, Pac3 int,
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

INSERT INTO SpotData SELECT
        Timestamp, Serial,
	Pdc1, Pdc2, 0,
	Idc1, Idc2, 0.0,
	Udc1, Udc2, 0.0,
	Pac1, Pac2, Pac3, Iac1, Iac2, Iac3, Uac1, Uac2, Uac3,
	EToday, ETotal, Frequency, OperatingTime, FeedInTime,
	BT_Signal, Status, GridRelay, Temperature
FROM SpotDataBackup;

DROP VIEW IF EXISTS vwSpotData;

CREATE View vwSpotData AS
SELECT datetime(Dat.TimeStamp, 'unixepoch', 'localtime') TimeStamp, 
        datetime(CASE WHEN (Dat.TimeStamp % 300) < 150
        THEN Dat.TimeStamp - (Dat.TimeStamp % 300)
        ELSE Dat.TimeStamp - (Dat.TimeStamp % 300) + 300
        END, 'unixepoch', 'localtime') AS Nearest5min,
        Inv.Name,
        Inv.Type,
        Dat.Serial,
        Pdc1,Pdc2,Pdc3,
        Idc1,Idc2,Idc3,
        Udc1,Udc2,Udc3,
        Pac1,Pac2,Pac3,
        Iac1,Iac2,Iac3,
        Uac1,Uac2,Uac3,
        Pdc1+Pdc2+Pdc3 AS PdcTot,
        Pac1+Pac2+Pac3 AS PacTot,
        CASE WHEN Pdc1+Pdc2+Pdc3 = 0 THEN
            0
        ELSE
            CASE WHEN Pdc1+Pdc2+Pdc3>Pac1+Pac2+Pac3 THEN
                ROUND(1.0*(Pac1+Pac2+Pac3)/(Pdc1+Pdc2+Pdc3)*100,1)
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
        FROM [SpotData] Dat
INNER JOIN Inverters Inv ON Dat.Serial = Inv.Serial
ORDER BY Dat.Timestamp Desc;

DROP VIEW IF EXISTS vwAvgSpotData;

CREATE VIEW vwAvgSpotData AS
       SELECT nearest5min,
              serial,
              avg(Pdc1) AS Pdc1,
              avg(Pdc2) AS Pdc2,
              avg(Pdc3) AS Pdc3,
              avg(Idc1) AS Idc1,
              avg(Idc2) AS Idc2,
              avg(Idc3) AS Idc3,
              avg(Udc1) AS Udc1,
              avg(Udc2) AS Udc2,
              avg(Udc3) AS Udc3,
              avg(Pac1) AS Pac1,
              avg(Pac2) AS Pac2,
              avg(Pac3) AS Pac3,
              avg(Iac1) AS Iac1,
              avg(Iac2) AS Iac2,
              avg(Iac3) AS Iac3,
              avg(Uac1) AS Uac1,
              avg(Uac2) AS Uac2,
              avg(Uac3) AS Uac3,
              avg(Temperature) AS Temperature
        FROM vwSpotData
        GROUP BY serial, nearest5min;
