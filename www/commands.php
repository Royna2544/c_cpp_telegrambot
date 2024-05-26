<!DOCTYPE html>

<title>C/C++ TgBot Commands</title>

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
    <link rel="stylesheet" href="commands.css" />
    <?php include 'navigation_bar.php'; ?>

    <table border="1px">
        <thead>
            <tr>
                <th>Command Name</th>
                <th>Description</th>
                <th>Usage</th>
                <th>Owner Only</th>
                <th>Notes</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td>alive</td>
                <td>Checks if bot is up, and sends information page for this bot.</td>
                <td>/alive</td>
                <td rowspan="2">No</td>
                <td rowspan="2">None</td>
            </tr>
            <tr>
                <td>start</td>
                <td>Alias for alive, used by Telegram bot start button</td>
                <td>/start</td>
            </tr>
            <tr>
                <td>bash</td>
                <td rowspan="2">Run bash commands and sends the result to chat</td>
                <td>/bash echo hi</td>
                <td rowspan="7">Yes</td>
                <td>Has execution timer of 10s</td>
            </tr>
            <tr>
                <td>ubash</td>
                <td>/ubash sleep 20</td>
                <td>None</td>
            </tr>
            <tr>
                <td>clone</td>
                <td>Clones identity of another user</td>
                <td>&lt;in-reply-to-user&gt; /clone</td>
                <td>Quite broken</td>
            </tr>
            <tr>
                <td>cmd</td>
                <td>Load/unload a command, controls access to command module globally</td>
                <td>/cmd bash unload | /cmd bash load</td>
                <td>None</td>
            </tr>
            <tr>
                <td>c, cpp, python, go</td>
                <td>Runs C/C++/Python/Go interpreter/compiler from replied-to message</td>
                <td>&lt;in-reply-to-c-code&gt; /c -O3 -fno-omit-frame-pointer</td>
                <td>If the host doesn't have the components for running, it is replaced with empty stub</td>
            </tr>
            <tr>
                <td>database</td>
                <td>Add users to whitelist/blacklist of the bot</td>
                <td>&lt;in-reply-to-user&gt; /database whitelist add | /database blacklist remove</td>
                <td rowspan="6">None</td>
            </tr>
            <tr>
                <td>saveid</td>
                <td>Saves a media's file id for later use (SocketCli, MediaCli)</td>
                <td>&lt;in-reply-to-media&gt; /saveid kys</td>
            </tr>
            <tr>
                <td>decho</td>
                <td>Delete the command message, and the bot sends the text instead</td>
                <td>/decho kys</td>
                <td rowspan="8">No</td>
            </tr>
            <tr>
                <td>decide</td>
                <td>Decide a statement (Yes/No)</td>
                <td>/decide Add MySQL support to this bot?</td>
            </tr>
            <tr>
                <td>delay</td>
                <td>Ping the bot and get network statistics</td>
                <td>/delay</td>
            </tr>
            <tr>
                <td>fileid</td>
                <td>Get fileid used by Telegram APIs for the media</td>
                <td>&lt;in-reply-to-media&gt; /fileid</td>
            </tr>
            <tr>
                <td>flash</td>
                <td>Emulate flashing a zip file in the provided name as in TWRP</td>
                <td>&lt;in-reply-to-text&gt; /flash or /flash supersu</td>
                <td>There is a rare chance for success, mostly it will fail</td>
            </tr>
            <tr>
                <td>ibash</td>
                <td>Interactive bash commands, like opening a terminal in telegram</td>
                <td>/ibash echo hi</td>
                <td>Quite Broken, Unix only</td>
            </tr>
            <tr>
                <td>possibility</td>
                <td>Chooses random possibilities from the message</td>
                <td>/possibility Kys\nKms\nSay hi</td>
                <td>Must be sperated by newlines, and more than one entry</td>
            </tr>
            <tr>
                <td>randsticker</td>
                <td>Chooses random sticker from the replied-to-sticker pack</td>
                <td>&lt;in-reply-to-sticker&gt; /randsticker</td>
                <td>None</td>
            </tr>
            <tr>
                <td>restart</td>
                <td>Restarts the bot</td>
                <td>/restart</td>
                <td rowspan="5">Yes</td>
                <td>Unix only</td>
            </tr>
            <tr>
                <td>rtload</td>
                <td>RunTime load command</td>
                <td>&lt;in-reply-to-cpp-code&gt; /rtload</td>
                <td>Broken</td>
            </tr>
            <tr>
                <td>spam</td>
                <td>Spams media or text</td>
                <td>&lt;in-reply-to-media-or-textr&gt; /spam 6 or /spam 10 kys</td>
                <td>Max spam count is 10, above that is converted to 10</td>
            </tr>
            <tr>
                <td>starttimer</td>
                <td>Start timer of the bot</td>
                <td>/starttimer 1h 2s</td>
                <td rowspan="2">Max time is 2h, the bot pins the msg if it can</td>
            </tr>
            <tr>
                <td>stoptimer</td>
                <td>Stop timer of the bot</td>
                <td>/stoptimer</td>
            </tr>
        </tbody>
    </table>
    <aside>
        <?php include 'gotop_button.php'; ?>
    </aside>
    <script type="text/javascript" src="navigation_bar.js"></script>
    <script type="text/javascript" src="chip/chip.js"></script>
</body>