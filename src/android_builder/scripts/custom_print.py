import os

def custom_print(*args, **kwargs):
    logfd = int(os.environ['PYTHON_LOG_FD'])

    # Get kwargs or set default values
    sep = kwargs.get('sep', ' ')
    end = kwargs.get('end', '\n')

    # Construct the message
    message = sep.join(map(str, args)) + end

    # Write the message to the file descriptor
    os.write(logfd, message.encode())
