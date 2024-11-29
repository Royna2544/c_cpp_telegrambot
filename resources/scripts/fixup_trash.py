"""
Fixup wrongly synced AOSP sources directory
"""
files = ['device',
         'art', 
         'cts', 
         'Android.bp', 
         'libcore', 
         'prebuilts', 
         'tools', 
         'bootable', 
         'WORKSPACE', 
         'hardware', 
         'packages', 
         'test', 
         'lk_inc.mk', 
         'developers', 
         #'build', Keep build/ as it is conflicting with CMake build
         '.repo', 
         'dalvik', 
         'bootstrap.bash', 
         'pdk', 
         'sdk', 
         'trusty', 
         'BUILD',
         'kernel', 
         'platform_testing', 
         'vendor',
         'Makefile', 
         'bionic', 
         'development', 
         'external', 
         'libnativehelper', 
         'toolchain', 
         'frameworks',
         'manifest',
         'system', 
         'out']

import shutil
import os

# Remove the existing AOSP sources directory
for file in files:
    print(f'Removing {file}...')
    if os.path.exists(file):
        try:
            shutil.rmtree(file)
        except Exception as e:
            try:
                os.remove(file)
            except Exception as e:
                print(f'Failed to remove {file}: {str(e)}')
    print('Done!')