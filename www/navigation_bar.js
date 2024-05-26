const displayKey = 'displayed';

$(() => {
    $(this).data(displayKey, false);
    $('#nav_show').click(() => {
        const shown = $(this).data(displayKey);
        const panel = $('#nav_panel')[0];
        if (shown) {
            panel.style.display = 'none';
        } else {
            panel.style.display = 'block';
        }
        $(this).data(displayKey, !shown);
    })
})