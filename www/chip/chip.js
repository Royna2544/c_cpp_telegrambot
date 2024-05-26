$(() => {
    let chips = document.querySelectorAll('.chip_container');
    for (let i = 0; i < chips.length; i++) {
        let chip = $(chips[i]);
        let destURL = chip.attr('url');
        if (destURL === "") {
            chip.addClass('chip_container_nolink');
        } else if (window.location.href.includes(destURL)) {
            chip.addClass('chip_container_active');
        } else {
            $(chip).click(() => {
                if (destURL.startsWith('https://')) {
                    window.open(destURL, '_blank');
                } else {
                    window.location.href = destURL;
                }
            })
        }
    }
})