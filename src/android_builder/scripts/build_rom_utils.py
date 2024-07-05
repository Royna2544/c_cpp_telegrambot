import subprocess_utils
import os
import print

print = print.print_fd

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
    
    command_list = [
        '. build/envsetup.sh',
        f'lunch {vendor}_{device}-{variant}',
        f'm {target} -j{jobs}'
    ]
    return subprocess_utils.run_command(' && '.join(command_list))