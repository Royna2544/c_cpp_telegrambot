import os
import requests
import custom_print
import subprocess_utils
from requests.exceptions import SSLError

print = custom_print.custom_print

def upload_to_gofile(device_name, file_prefix) -> str:
    # Define the directory to search for files
    directory = f"./out/target/product/{device_name}"

    # Ensure the directory exists
    if not os.path.exists(directory):
        return f"The directory {directory} does not exist."

    # Find the first file matching the prefix
    file_to_upload = None
    for filename in os.listdir(directory):
        if filename.startswith(file_prefix) and filename.endswith('.zip'):
            file_to_upload = os.path.join(directory, filename)
            break

    if not file_to_upload:
        return f"No file found with prefix {file_prefix}."

    # GoFile API URL
    gofile_api_url = "https://api.gofile.io/uploadFile"

    try:
        # Open the file in binary mode
        with open(file_to_upload, 'rb') as file:
            files = {'file': (filename, file)}

            # Upload the file to GoFile
            response = requests.post(gofile_api_url, files=files)
    except SSLError as e:
        print(f"SSL certificate verification failed: {e}")
        dir_path = os.path.dirname(os.path.realpath(__file__))
        out, err = subprocess_utils.run_command_with_output(f'bash {dir_path}/upload_file.bash {file_to_upload}')
        return f"""
    STDOUT: {out}
    STDERR: {err}
    """

    # Check the response from GoFile
    if response.status_code == 200:
        result = response.json()
        if result['status'] == 'ok':
            return f"File uploaded successfully. Download link: {result['data']['downloadPage']}"
        else:
            return f"Failed to upload file: {result['message']}"
    else:
        return f"Failed to connect to GoFile. Status code: {response.status_code}"
