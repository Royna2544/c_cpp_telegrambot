import subprocess
import sys
import custom_print

testing = False
testingRet = True

print = custom_print.custom_print

def run_command_with_output(command: str):
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
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        
        # Return output and error as strings
        output = result.stdout.decode('utf-8')
        error = result.stderr.decode('utf-8')
        print("Exit status: %d" % result.returncode)
        return output, error
    
    except subprocess.CalledProcessError as e:
        print(str(e))
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
