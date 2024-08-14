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
    
    shell_process = subprocess.Popen(['bash'], stdin=subprocess.PIPE, text=True)
    
    def write(s: str):
        print('Writing \'%s\'' % s)
        shell_process.stdin.write(s + '\n')
        shell_process.stdin.flush()
        
    write('. build/envsetup.sh')
    write('unset USE_CCACHE')
    write(f'lunch {vendor}_{device}-{variant}')
    write(f'm {target} -j{jobs}')
    shell_process.stdin.close()
    print('Now waiting...')
    shell_process.wait()

    return shell_process.returncode == 0