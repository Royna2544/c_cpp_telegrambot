<?php
require_once 'php/echo_and_unset.php';
?>

<div class="descbox_container">
    <div class="descbox_title">
        <img src="<?php echo_and_unset($descbox_image) ?>" />
        <h1>
            <?php
            echo_or_default($descbox_title, 'Title not set');
            ?>
        </h1>
    </div>
    <div class="descbox_content">
        <?php if (isset($descbox_desc)) : ?>
            <p>
                <?php
                echo_and_unset($descbox_desc);
                ?>
            </p>
        <?php elseif (isset($descbox_resource)) : ?>
            <?php include $descbox_resource;
            unset($descbox_resource);
            ?>
        <?php endif; ?>
    </div>
</div>