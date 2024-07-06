import subprocess
import sys
import custom_print

testing = False
testingRet = True

print = custom_print.custom_print

def run_command_with_output(command: str, testing: bool = False, testingRet: bool = False):
    print("Running command: %s" % command)
    sys.stdout.flush()
    
    if testing:
        if testingRet:
            return 'SomeOutput', 'SomeError'
        else:
            return None, None

    try:
        # Use subprocess.run instead of subprocess.check_call for capturing output
        result = subprocess.run(command, shell=True, executable="/bin/bash",
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # Check return code to handle errors
        if result.returncode != 0:
            errstr = f"Command '{command}' failed with exit code {result.returncode}\n"
            errstr += result.stderr.decode('utf-8')
            print(errstr)
            return None, result.stderr.decode('utf-8')
        
        # Return output and error as strings
        output = result.stdout.decode('utf-8')
        error = result.stderr.decode('utf-8')
        return output, error
    
    except subprocess.CalledProcessError as e:
        errstr = f"Command '{command}' failed with exception {str(e)}\n"
        print(errstr)
        return None, str(e)
    
    except Exception as e:
        errstr = f"Exception occurred while running command '{command}': {str(e)}\n"
        print(errstr)
        return None, str(e)
    
    finally:
        sys.stdout.flush()
        sys.stderr.flush()
    
def run_command(command: str) -> bool:
    s, _ = run_command_with_output(command)
    return s is not None
