-- User
SELECT userid, info FROM usermap WHERE info != 0; -- Owner id = 0

-- Media
SELECT mediaids.mediaid, mediaids.mediauniqueid, medianame.name, mediamap.mediatype
FROM mediamap
INNER JOIN medianame ON mediamap.medianameid = medianame.id
INNER JOIN mediaids ON mediamap.mediaid = mediaids.id;

SELECT chatid, chatname FROM chatmap