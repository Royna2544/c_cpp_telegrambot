<!DOCTYPE html>

<title>C/C++ TgBot Details</title>

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" type="image/png" href="favicon.ico" />
    <script type="text/javascript" src="https://code.jquery.com/jquery-3.7.1.min.js"></script>
</head>

<body>
    <link rel="stylesheet" href="navigation_bar.css" />
    <link rel="stylesheet" href="chip/chip.css" />
    <link rel="stylesheet" href="gotop_button.css" />
    <link rel="stylesheet" href="detailed_info.css" />

    <?php include 'navigation_bar.php'; ?>
    <table>
        <thead>
            <td colspan="2" class="table_title">Used languages</td>
        </thead>
        <tbody>

            <tr>
                <th>
                    <?php
                    $chip_img_path = 'resources/devicons/cplusplus-original.svg';
                    $chip_name = 'C++';
                    $chip_url = '';
                    include "chip/include_chip.php"
                    ?>
                </th>
                <td>Used for the main C++ project</td>
            </tr>
            <tr>
                <th>
                    <?php
                    $chip_img_path = 'resources/devicons/c-original.svg';
                    $chip_name = 'C';
                    $chip_url = '';
                    include "chip/include_chip.php"
                    ?>
                </th>
                <td>
                    src/popen_wdt contains popen(2) wrapper
                </td>
            </tr>
            <tr>
                <th>
                    <?php
                    $chip_img_path = 'resources/devicons/kotlin-original.svg';
                    $chip_name = 'Kotlin';
                    include "chip/include_chip.php"
                    ?>
                </th>
                <td>Used for Android app client</td>
            </tr>
            <tr>
                <th>
                    <?php
                    $chip_img_path = 'resources/devicons/javascript-original.svg';
                    $chip_name = 'Javascript';
                    include "chip/include_chip.php"
                    ?>
                </th>
                <td>Webpage's codes</td>
            </tr>
            <tr>
                <th>
                    <?php
                    $chip_img_path = 'resources/devicons/php-original.svg';
                    $chip_name = 'PHP';
                    include "chip/include_chip.php"
                    ?>
                </th>
                <td>Webpage's HTML generation</td>
            </tr>
            <tr>
                <td>
                    <?php
                    $chip_img_path = 'resources/devicons/sass-original.svg';
                    $chip_name = 'SASS';
                    include "chip/include_chip.php"
                    ?>
                </td>
                <td>Webpage's CSS generation</td>
            </tr>
        </tbody>
    </table>
    <script type="text/javascript" src="navigation_bar.js"></script>
    <script type="text/javascript" src="chip/chip.js"></script>
</body>