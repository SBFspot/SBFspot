CREATE TABLE WeatherData (
    UTC           INT (4) NOT NULL,
    WeatherID     INT (2),
    Temperature   FLOAT,
    Pressure      FLOAT,
    Humidity      FLOAT,
    Visibility    FLOAT,
    WindSpeed     FLOAT,
    WindDirection FLOAT,
    PVoutput      INT (1),
    PRIMARY KEY (UTC)
)
WITHOUT ROWID;

CREATE View vwWeatherData AS
SELECT
	datetime(UTC, 'unixepoch', 'localtime') AS LocalDateTime,
	WeatherID,
	Temperature,
	Pressure,
	Humidity,
	Visibility,
	WindSpeed,
	WindDirection,
	PVoutput
FROM WeatherData;
