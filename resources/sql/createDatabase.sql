-- info 0: Owner, 1: Blacklisted, 2: Whitelisted
CREATE TABLE usermap (
    userid BIGINT NOT NULL PRIMARY KEY,
    info INT NOT NULL
);

CREATE TABLE mediaids (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mediauniqueid VARCHAR(20) NOT NULL,
    mediaid VARCHAR(70) NOT NULL,
    UNIQUE (mediauniqueid)
);

CREATE TABLE medianame (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL,
    UNIQUE (name)
);

CREATE TABLE mediamap (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    medianameid INTEGER NOT NULL,
    mediaid INTEGER NOT NULL,
    FOREIGN KEY (medianameid) REFERENCES medianame(id) ON DELETE CASCADE,
    FOREIGN KEY (mediaid) REFERENCES mediaids(id) ON DELETE CASCADE
);

CREATE TABLE chatmap (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    chatid BIGINT NOT NULL,
    chatname VARCHAR(100) NOT NULL,
    UNIQUE (chatid)
)