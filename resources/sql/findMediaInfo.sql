SELECT mediaids.mediaid, mediaids.mediauniqueid, mediamap.mediatype
FROM mediamap
INNER JOIN medianame ON mediamap.medianameid = medianame.id
INNER JOIN mediaids ON mediamap.mediaid = mediaids.id
WHERE medianame.name = ?
