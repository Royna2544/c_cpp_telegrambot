<?php
require_once 'php/echo_and_unset.php';

if (file_exists($filename)) : ?>
    <div class="green_box">
        <pre><code class="language-cpp">
<?php
    $str = (string) file_get_contents($filename);
    // Replace certain characters with HTML escapers
    $str = str_replace('&', '&amp;', $str);
    $str = str_replace('<', '&lt;', $str);
    $str = str_replace('>', '&gt;', $str);
    echo_and_unset($str);
?>
        </code></pre>
        <script src="cpp_code/prism.js"></script>
    </div>
<?php else : ?>
    <script type="text/javascript">
        <?php echo "alert(\"Sorry, failed to load: " . $filename . "\")" ?>
    </script>
<?php endif; ?>