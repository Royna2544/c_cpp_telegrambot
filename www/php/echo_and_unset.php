<?php
/**
 * This function echoes the value of the given variable and then unsets it.
 *
 * @param mixed &$value The variable to be echoed and unset.
 *
 * @return void This function does not return any value.
 *
 * @throws Exception If the given variable is not set.
 */
function echo_and_unset(&$value) {
    // Check if the variable is set
    if (!isset($value)) {
        throw new Exception('The given variable is not set.');
    }

    // Echo the value of the variable
    echo $value;

    // Unset the variable
    unset($value);
}

/**
 * This function echoes the value of the given variable and then unsets it.
 * If the variable is not set, it will be set to the provided default value.
 *
 * @param mixed &$value The variable to be echoed and unset.
 * @param mixed $default The default value to set if the variable is not set.
 *
 * @return void This function does not return any value.
 *
 * @throws Exception If the given variable is not set.
 */
function echo_or_default(&$value, $default) {
    // Check if the variable is set
    if (!isset($value)) {
        $value = $default;
    }
    echo_and_unset($value);
}