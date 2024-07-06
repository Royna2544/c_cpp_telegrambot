import os

def get_logfd() -> int:
    """
    This function retrieves the file descriptor for logging.

    Returns:
    int: The file descriptor for logging.

    Raises:
    OSError: If the environment variable 'PYTHON_LOG_FD' is not set or cannot be converted to an integer.
    """
    try:
        logfd = int(os.environ['PYTHON_LOG_FD'])
        return logfd
    except KeyError:
        raise OSError("Environment variable 'PYTHON_LOG_FD' not set")
    except ValueError:
        raise OSError("Invalid value for environment variable 'PYTHON_LOG_FD'")

def custom_print(*args, **kwargs):
    """
    This function prints the provided arguments to a file descriptor specified by the environment variable 'PYTHON_LOG_FD'.

    Parameters:
    *args: Variable length argument list. The values to be printed.
    **kwargs: Arbitrary keyword arguments.
        sep: String inserted between values, default is a single space.
        end: String appended after the last value, default is a newline.

    Returns:
    None

    Raises:
    OSError: If the file descriptor is invalid or not open for writing.
    """
    logfd = get_logfd()

    # Get kwargs or set default values
    sep = kwargs.get('sep', ' ')
    end = kwargs.get('end', '\n')

    # Construct the message
    message = sep.join(map(str, args)) + end

    # Write the message to the file descriptor
    os.write(logfd, message.encode())
