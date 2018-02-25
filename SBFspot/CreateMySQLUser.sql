#DROP USER 'SBFspotUser'@'localhost';
CREATE USER 'SBFspotUser'@'localhost' IDENTIFIED BY 'SBFspotPassword';
GRANT DELETE,INSERT,SELECT,UPDATE ON SBFspot.* TO 'SBFspotUser'@'localhost';
FLUSH PRIVILEGES;