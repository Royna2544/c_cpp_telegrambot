import subprocess
import sys
import custom_print

testing = False
testingRet = True

print = custom_print.custom_print

def run_command(command: str) -> bool:
    print("Running command: %s" % command)
    sys.stdout.flush()
    ret = 0
    
    if testing:
        return testingRet
    try:
        subprocess.check_call(command, shell=True, executable="/bin/bash")
    except subprocess.CalledProcessError as e:
        errstr = f"Command '{command}' failed with exit code {e.returncode}"
        print(errstr)    
        ret = e.returncode
    finally:
        sys.stdout.flush()
        sys.stderr.flush()
    return ret == 0