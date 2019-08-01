CREATE Table SpotDataBackup (
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

INSERT INTO SpotDataBackup select * from SpotData;

DROP TABLE SpotData;

CREATE Table SpotData (
        TimeStamp int(4) NOT NULL,
        Serial int(4) NOT NULL,
        Pdc1 int(4), Pdc2 int(4), Pdc3 int(4),
        Idc1 float, Idc2 float, Idc3 float,
        Udc1 float, Udc2 float, Udc3 float,
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
    Select From_UnixTime(Dat.TimeStamp) AS TimeStamp,
    From_UnixTime(CASE WHEN (Dat.TimeStamp % 300) < 150
    THEN Dat.TimeStamp - (Dat.TimeStamp % 300)
    ELSE Dat.TimeStamp - (Dat.TimeStamp % 300) + 300
    END) AS Nearest5min,
    Inv.Name,
    Inv.Type,
    Dat.Serial,
    Pdc1, Pdc2, Pdc3,
    Idc1, Idc2, Idc3,
    Udc1, Udc2, Udc3,
    Pac1, Pac2, Pac3,
    Iac1, Iac2, Iac3,
    Uac1, Uac2, Uac3,
    Pdc1+Pdc2+Pdc3 AS PdcTot,
    Pac1+Pac2+Pac3 AS PacTot,
    CASE WHEN Pdc1+Pdc2+Pdc3 = 0 THEN
        0
    ELSE
        CASE WHEN Pdc1+Pdc2+Pdc3>Pac1+Pac2+Pac3 THEN
            ROUND((Pac1+Pac2+Pac3)/(Pdc1+Pdc2+Pdc3)*100,1)
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

DROP VIEW IF EXISTS vwAvgSpotData;

CREATE VIEW vwAvgSpotData AS
       SELECT nearest5min,
              serial,
              cast(avg(Pdc1) as decimal(9)) AS Pdc1,
              cast(avg(Pdc2) as decimal(9)) AS Pdc2,
              cast(avg(Pdc3) as decimal(9)) AS Pdc3,
              cast(avg(Idc1) as decimal(9,3)) AS Idc1,
              cast(avg(Idc2) as decimal(9,3)) AS Idc2,
              cast(avg(Idc3) as decimal(9,3)) AS Idc3,
              cast(avg(Udc1) as decimal(9,2)) AS Udc1,
              cast(avg(Udc2) as decimal(9,2)) AS Udc2,
              cast(avg(Udc3) as decimal(9,2)) AS Udc3,
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
