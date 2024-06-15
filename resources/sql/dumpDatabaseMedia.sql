SELECT mediamap.id, mediamap.uniqueid, medianames.name
FROM mediamap
INNER JOIN medianames ON mediamap.nameid = medianames.id
