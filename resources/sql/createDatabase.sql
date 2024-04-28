-- info 0: Owner, 1: Blacklisted, 2: Whitelisted
CREATE TABLE usermap (
    userid BIGINT NOT NULL PRIMARY KEY,
    info INT NOT NULL
);

INSERT INTO usermap VALUES (1185607882, 0);

CREATE TABLE mediamap (
    uniqueid VARCHAR(255) NOT NULL,
    id VARCHAR(255) NOT NULL,
    nameid INTEGER NOT NULL,
    PRIMARY KEY (uniqueid)
);

CREATE TABLE medianames (
    id INTEGER NOT NULL,
    name VARCHAR(255) NOT NULL,
    PRIMARY KEY (id)
);