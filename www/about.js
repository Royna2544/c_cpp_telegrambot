const Vote = {
    up: "up",
    down: "down",
    none: "none"
};
Object.freeze(Vote);

function updateThumbs(vote) {
    const Tup = $('#tup');
    const Tdn = $('#tdn');
    var upPic, downPic;
    switch (vote) {
        case Vote.up:
            upPic = 'thumb_up_filled';
            downPic = 'thumb_down';
            break;
        case Vote.down:
            upPic = 'thumb_up';
            downPic = 'thumb_down_filled';
            break;
        case Vote.none:
            upPic = 'thumb_up';
            downPic = 'thumb_down';
            break;
    }
    Tup.attr('src', '/resources/google/' + upPic + '.svg');
    Tdn.attr('src', '/resources/google/' + downPic + '.svg');
}

function sendToEndPoint(vote) {
    // Send POST request
    $.ajax({
        url: '/api/votes',
        type: 'POST',
        data: JSON.stringify({ votes: vote }),
        contentType: 'application/json', // Set the content type to JSON
        success: function(response) {
            // Handle success response
            console.log('Success:', response);
        },
        error: function(xhr, status, error) {
            // Handle error response
            console.error('Error:', error);
        }
    });
}

$(function() {
    var voted = Vote.none;
    const Tup = $('#tup');
    const Tdn = $('#tdn');

    Tup.click(function() {
        switch (voted) {
            case Vote.none:
            case Vote.down:
                voted = Vote.up;
                break;
            case Vote.up:
                voted = Vote.none;
                break;
        }
        updateThumbs(voted);
        sendToEndPoint(voted);
    });
    Tdn.click(function() {
        switch (voted) {
            case Vote.none:
            case Vote.up:
                voted = Vote.down;
                break;
            case Vote.down:
                voted = Vote.none;
                break;
        }
        updateThumbs(voted);
        sendToEndPoint(voted);
    });
    updateThumbs(Vote.none);
});
