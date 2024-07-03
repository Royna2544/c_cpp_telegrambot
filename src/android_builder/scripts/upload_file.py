import os
import requests

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

    # Open the file in binary mode
    with open(file_to_upload, 'rb') as file:
        files = {'file': (filename, file)}

        # Upload the file to GoFile
        response = requests.post(gofile_api_url, files=files)

    # Check the response from GoFile
    if response.status_code == 200:
        result = response.json()
        if result['status'] == 'ok':
            return f"File uploaded successfully. Download link: {result['data']['downloadPage']}"
        else:
            return f"Failed to upload file: {result['message']}"
    else:
        return f"Failed to connect to GoFile. Status code: {response.status_code}"
