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
