<!DOCTYPE html>
<html>
<title>C/C++ TgBot (PHP)</title>

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" type="image/png" href="favicon.ico" />
    <script type="text/javascript" src="https://code.jquery.com/jquery-3.7.1.min.js"></script>
</head>

<body>
    <!-- Include CSS area -->
    <link rel="stylesheet" href="about.css" />
    <link rel="stylesheet" href="cpp_code/prism.css" />
    <link rel="stylesheet" href="desc_box/descbox.css" />
    <link rel="stylesheet" href="chip/chip.css" />
    <link rel="stylesheet" href="navigation_bar.css" />
    <link rel="stylesheet" href="gotop_button.css" />

    <!-- Body content -->
    <?php include 'navigation_bar.php'; ?>
    <section>
        <div class="container_vertical">
            <div class="container_desc_box">
                <?php
                $descbox_title = 'Supported OS';
                $descbox_desc = 'Windows MSYS2, Linux, macOS x86/ARM64';
                $descbox_image = 'resources/google/desktop.svg';
                include "desc_box/include_descbox.php"
                ?>
                <?php
                $descbox_title = 'Languages';
                $descbox_desc = 'C, C++, PHP, SQL, Kotlin';
                $descbox_image = 'resources/google/language.svg';
                include "desc_box/include_descbox.php"
                ?>
                <?php
                $descbox_title = 'Design goals';
                $descbox_desc = 'Never ending development to learn new stuff';
                $descbox_image = 'resources/google/flag.svg';
                include "desc_box/include_descbox.php"
                ?>
                <?php
                $descbox_title = 'Time passed';
                $startT = new DateTime('2023-02-16 17:55:00');
                $endT = new DateTime('now');
                $interval = $startT->diff($endT);
                $descbox_desc = $interval->format('%y years, %m months, %d days, %h hours, %i minutes');
                $descbox_image = 'resources/google/schedule.svg';
                include "desc_box/include_descbox.php"
                ?>
            </div>
        </div>
        <div class="divider"></div>
        <div class="container_vertical">
            <div class="green_box container_inline">
                <img class="highlighted_img" src="resources/ISO_CPP_Logo.svg">
                <h1>+</h1>
                <img class="highlighted_img" src="resources/telegram.svg">
            </div>
            <div class="text_block">
                <h1>Telegram and C++</h1>
                <h4>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nunc iaculis bibendum facilisis. Vivamus urna nulla, fringilla eget lectus id, suscipit fringilla augue. Aliquam sed rutrum lectus. Vivamus fringilla luctus magna, ut blandit augue. Cras tincidunt, nisi ac dignissim commodo, nisi dui pellentesque ipsum, malesuada eleifend sem turpis ut turpis. Donec eget dolor commodo, aliquet mauris et, condimentum leo. Sed egestas vitae quam ac convallis. Pellentesque nec metus sed dui rhoncus gravida. Aliquam faucibus, purus sed tincidunt pellentesque, magna ligula ullamcorper sapien, sed imperdiet est ipsum at ante. Nullam non libero iaculis, rutrum leo id, vehicula neque. In elit sapien, ornare eget mollis non, consectetur at massa. Etiam in felis sed dolor consectetur posuere quis a magna. Donec efficitur, tellus sed mattis dictum, turpis lorem ultricies lacus, eu suscipit libero lacus ac magna. Suspendisse mollis elementum ligula ac consectetur. Integer euismod varius purus, sit amet porta nulla posuere vitae.</h4>
            </div>
        </div>
        <div class="divider"></div>
        <div class="container_vertical">
            <?php
            $filename = "resources/TgBot.cpp";
            include "cpp_code/include_cppcode.php";
            ?>
            <div class="text_block">
                <h1>Code</h1>
                <h3>Sed porta aliquam malesuada. Nunc arcu justo, aliquet quis quam id, pharetra congue leo. Praesent faucibus velit lorem, eget varius quam convallis non. Fusce condimentum, mi eget tristique porttitor, diam orci tincidunt augue, quis tincidunt eros quam sed nibh. Nam tempus orci a sapien iaculis varius. Suspendisse mattis mollis sollicitudin. Maecenas a arcu ac diam luctus rhoncus vel quis justo. Duis eget mi at diam facilisis finibus. Curabitur eu consequat orci. Vivamus luctus feugiat dolor, in pellentesque justo ultrices non. Suspendisse potenti. Duis venenatis augue eu nisi molestie, quis ultrices arcu pulvinar.</h3>
            </div>
        </div>
    </section>
    <aside>
        <?php include 'gotop_button.php'; ?>
    </aside>
    <div class="divider"></div>
    <footer>
        <div class="container_end green_box">
            <h3>Did you like this website?</h3>
            <input type="image" class="small_img" id="tup" />
            <input type="image" class="small_img" id="tdn" />
        </div>
    </footer>

    <!-- Include JavaScript area -->
    <script type="text/javascript" src="desc_box/descbox.js"></script>
    <script type="text/javascript" src="about.js"></script>
    <script type="text/javascript" src="navigation_bar.js"></script>
    <script type="text/javascript" src="chip/chip.js"></script>
</body>

</html>