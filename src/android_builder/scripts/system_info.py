import os
import platform
import subprocess

def bytes_to_human_readable(num):
    """Converts bytes to a human-readable format."""
    for unit in ['B', 'KB', 'MB', 'GB', 'TB', 'PB']:
        if num < 1024:
            return f"{num:.2f} {unit}"
        num /= 1024

# Function to detect WSL version
def is_wsl(v: str = platform.uname().release) -> int:
    """Detects if Python is running in WSL."""
    if v.endswith("-Microsoft"):
        return 1
    elif v.endswith("microsoft-standard-WSL2"):
        return 2
    return 0

def get_cpu_info():
    cpu_info = {}

    # Get CPU model
    try:
        if platform.system() == 'Linux':
            with open('/proc/cpuinfo') as f:
                cpuinfo = f.read()
            for line in cpuinfo.split('\n'):
                if 'model name' in line:
                    cpu_info['CPU::Model'] = line.split(':')[1].strip()
                    break
    except Exception as e:
        cpu_info['CPU::Model'] = f"Could not determine CPU model: {e}"

    # Get number of CPU cores
    try:
        cpu_info['CPU::Cores'] = os.cpu_count()
    except Exception as e:
        cpu_info['CPU::Cores'] = f"Could not determine CPU cores: {e}"

    return cpu_info

def get_memory_info():
    meminfo = {}
    with open('/proc/meminfo', 'r') as f:
        for line in f:
            parts = line.split()
            key = parts[0].rstrip(':')
            value = int(parts[1])
            # Map it to MB
            value /= 1024
            meminfo[key] = value

    meminfo['UsedMem'] = meminfo['MemTotal'] - meminfo['MemFree'] 
    meminfo['UsedMem'] -= meminfo['Buffers'] + meminfo['Cached']
    
    return meminfo

def to_gbytes(megabytes: str) -> str:
    return f"{round(megabytes / 1024, 2)} GB"

def get_vital_memory_info(withliteral: bool = True):
    meminfo = get_memory_info()

    # Calculate used percentage
    used_percentage = (meminfo['UsedMem'] / meminfo['MemTotal']) * 100

    return {
        'Memory::Used': to_gbytes(meminfo['UsedMem'])
            if withliteral else meminfo['UsedMem'],
        'Memory::UsagePercent': f'{round(used_percentage, 2)}%'
            if withliteral else round(used_percentage, 2),
        'Memory::Total': to_gbytes(meminfo['MemTotal'])
            if withliteral else meminfo['MemTotal'],
    }
    
def get_disk_info():
    disk_info = {}

    # Get disk type (assuming / is on the first listed disk) if not in WSL
    if is_wsl() == 0:
        try:
            result = subprocess.run(['lsblk', '-o', 'NAME,ROTA,MOUNTPOINT'], capture_output=True, text=True)
            for line in result.stdout.splitlines():
                if 'NAME' in line:
                    continue
                if '/' in line.split()[-1]:
                    if '0' in line.split()[1]:
                        disk_info['disk_type'] = 'SSD'
                    else:
                        disk_info['disk_type'] = 'HDD'
                    break
        except Exception as e:
            disk_info['disk_type'] = f"Could not determine disk type: {e}"

    # Get storage left in /
    try:
        result = subprocess.run(['df', '-B1', '/'], capture_output=True, text=True)
        output = result.stdout.splitlines()[1].split()
        disk_info['StorageSpace::Free::/'] = bytes_to_human_readable(int(output[3]))  # Available space in bytes
    except Exception as e:
        disk_info['StorageSpace::Free::/'] = f"Could not determine storage left in /: {e}"

    # Get storage left in current working directory
    try:
        cwd = os.getcwd()
        result = subprocess.run(['df', '-B1', cwd], capture_output=True, text=True)
        output = result.stdout.splitlines()[1].split()
        disk_info[f"StorageSpace::Free::{cwd}"] = bytes_to_human_readable(int(output[3]))  # Available space in bytes
    except Exception as e:
        disk_info[f"StorageSpace::Free::{cwd}"] = f"Could not determine storage left in {cwd}: {e}"

    return disk_info

def get_system_info():
    info = {}

    # Get CPU info
    info.update(get_cpu_info())

    # Get memory info
    info.update(get_vital_memory_info(withliteral=True))

    # Get disk info
    info.update(get_disk_info())

    # Add WSL version information if running in WSL
    if is_wsl() != 0:
        info['WSL::Version'] = is_wsl()

    return info

def get_system_summary() -> str:
    vmem_info = get_vital_memory_info(withliteral=False)
    warnings = []

    # Check for low memory conditions
    if 'Memory::Total' in vmem_info:
        if vmem_info['Memory::Total'] < 16 * 1024:
            warnings.append("Really low memory detected. Android 13+ builds would be almost impossible.")
        elif vmem_info['Memory::Total'] < 32 * 1024:
            warnings.append("Low memory detected. You might consider adding some swap.")

    # Add WSL performance warning
    if is_wsl() != 0:
        warnings.append(f"This script is running in WSL {is_wsl()}. System performance may be affected.")

    # Prepare system info and warnings for output
    system_info_strs = '\n'.join([f"{key}: {value}" for key, value in get_system_info().items()])
    warning_strs = '\n'.join(['Warning: %s' % f for f in warnings])

    return system_info_strs + '\n\n' + warning_strs

def get_memory_usage() -> float:
    memory_info = get_vital_memory_info(withliteral=False)
    return memory_info['Memory::UsagePercent']

def get_memory_total() -> float:
    memory_info = get_vital_memory_info(withliteral=False)
    return memory_info['Memory::Total']

# Example usage
if __name__ == "__main__":
    summary = get_system_summary()
    print(summary)
