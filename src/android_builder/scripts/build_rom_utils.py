import subprocess_utils
import subprocess
import os
import custom_print

print = custom_print.custom_print

def find_vendor_str() -> str:
    vendor_path = 'vendor/'
    if not os.path.exists(vendor_path):
        print(f"Directory '{vendor_path}' does not exist.")
        return None
    
    for v in os.listdir(vendor_path):
        vendor_dir = os.path.join(vendor_path, v)
        common_mk_path = os.path.join(vendor_dir, 'config', 'common.mk')
        if os.path.isfile(common_mk_path):
            print(f'Found vendor: {v}')
            return v
    return None

def build_rom(device: str, variant: str, target: str, jobs: int) -> bool:
    vendor = find_vendor_str()
    
    if vendor is None:
        print('Couldn\'t find vendor')
        return False
    
    shell_process = subprocess.Popen(['bash'], stdin=subprocess.PIPE, 
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    print('Writing . build/envsetup.sh')
    shell_process.stdin.write('. build/envsetup.sh\n')
    if os.getenv('USE_CCACHE') is not None:
        print('ccache is useless, unset USE_CCACHE')
        os.unsetenv('USE_CCACHE')
    print(f'Writing lunch {vendor}_{device}-{variant}')
    shell_process.stdin.write(f'lunch {vendor}_{device}-{variant}\n')
    shell_process.stdin.write('echo "exit code: $?\n')
    shell_process.stdin.flush()
    print('Waiting for lunch command result...')
    while True:
        line = shell_process.stdout.readline()
        if line.startswith("exit code:"):
            # Extract the exit code from the line
            exit_code = int(line.strip().split()[-1])
            break
        print(line)
        
    if exit_code != 0:
        print(f'Error: lunch command failed with exit code {exit_code}')
        print(shell_process.stderr.read())
        return False
    
    # Remap to original stdout/stderr
    shell_process.stdout.close()
    shell_process.stderr.close()
    shell_process.stdout = os.fdopen(os.dup(1), 'w') 
    shell_process.stderr = os.fdopen(os.dup(2), 'w')
    
    # Start the build process
    print(f'Writing m {target} -j{jobs}')
    shell_process.stdin.write(f'm {target} -j{jobs}\n')
    shell_process.stdin.flush()
    shell_process.stdin.close()
    print('Now waiting...')
    shell_process.wait()
    return shell_process.returncode == 0