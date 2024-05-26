const expandedKey = 'expanded';
const animationDelay = 1000

class DescBox {
    constructor() {
        let parent = document.querySelectorAll(".descbox_container");
        if (parent.length == 0) {
            throw new Error('No descbox_container found in document');
        }
        // TODO: Do something else, not taking first array elements
        this.elementHeight = parent[0].children[0].offsetHeight;
        this.elementWidth = parent[0].children[0].offsetWidth;
    }
    updateHeight(container, expanded) {
        if (!container || !container.style) {
            console.error('Invalid container:', container);
            return;
        }
        let multiple = expanded ? 2 : 1;
        container.style.height = (multiple * this.elementHeight) + "px";
    }
    onClickCallback(titleElem) {
        const content = titleElem.parent().children(".descbox_content");
        let update;

        // padding 10
        if (titleElem.data(expandedKey)) {
            update = ['-=', false];
        } else {
            update = ['+=', true];
        }
        content.animate({ top: update[0] + (this.elementHeight + 10) + 'px' }, animationDelay);

        if (update[1]) {
            // Update immediately
            this.updateHeight(titleElem.parent().get(0), update[1]);
        } else {
            // Wait for animation to finish, then update height
            titleElem.addClass('block-touch');
            setTimeout(() => {
                this.updateHeight(titleElem.parent().get(0), update[1]);
                titleElem.removeClass('block-touch');
            }, animationDelay);
        }
        titleElem.data(expandedKey, update[1]);
    }
    registerOnClick() {
        var parent = document.querySelectorAll(".descbox_title");
        for (var i = 0; i < parent.length; i++) {
            let parElem = $(parent[i]);
            parElem.data(expandedKey, false);
            parElem.click(() => {
                this.onClickCallback(parElem);
            });
        }
    }
    static getInstance() {
        if (!DescBox.instance) {
            DescBox.instance = new DescBox();
        }
        return DescBox.instance;
    }
};

$(function () {
    try {
        let box = DescBox.getInstance();
        box.registerOnClick();
    } catch (error) {
        console.error(error);
    }
});
