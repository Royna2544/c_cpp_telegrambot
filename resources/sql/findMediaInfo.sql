SELECT mediamap.id, mediamap.uniqueid
FROM mediamap
INNER JOIN medianames ON mediamap.nameid = medianames.id
WHERE medianames.name = ?
