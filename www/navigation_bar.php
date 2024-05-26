<?php $GH_ROOT = "https://github.com/Royna2544/c_cpp_telegrambot/"; ?>

<header class="nav_centered_flex">
    <img id="nav_show" class="menu_img" src="resources/google/menu.svg">
    <nav class="aside_navbar" id="nav_panel">
        <h2 class="start_padded_text">Links</h2>
        <?php
        $chip_img_path = 'resources/google/home.svg';
        $chip_name = 'Home';
        $chip_url = "about.html";
        include "chip/include_chip.php"
        ?>
        <?php
        $chip_img_path = 'resources/google/library_books.svg';
        $chip_name = 'Commands';
        $chip_url = "commands.html";
        include "chip/include_chip.php"
        ?>
        <?php
        $chip_img_path = 'resources/google/info.svg';
        $chip_name = 'Information';
        $chip_url = "detailed_info.html";
        include "chip/include_chip.php"
        ?>
        <?php
        $chip_img_path = 'resources/github-mark.svg';
        $chip_name = 'Repository';
        $chip_url = $GH_ROOT;
        include "chip/include_chip.php"
        ?>
        <?php
        $chip_img_path = 'resources/github-mark.svg';
        $chip_name = 'Issues';
        $chip_url = $GH_ROOT . "issues";
        include "chip/include_chip.php"
        ?>
        <?php
        $chip_img_path = 'resources/github-mark.svg';
        $chip_name = 'License';
        $chip_url = $GH_ROOT . "blob/master/LICENSE";
        include "chip/include_chip.php"
        ?>
        <?php
        $chip_img_path = 'resources/github-mark.svg';
        $chip_name = 'Wiki';
        $chip_url = $GH_ROOT . "wiki";
        include "chip/include_chip.php"
        ?>
    </nav>
    <div class="title_text">
        <h1>A Telegram Bot, written in C/C++</h1>
    </div>
</header>