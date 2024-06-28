SELECT mediaids.mediaid, mediaids.mediauniqueid, medianame.name
FROM mediamap
INNER JOIN medianame ON mediamap.medianameid = medianame.id
INNER JOIN mediaids ON mediamap.mediaid = mediaids.id
