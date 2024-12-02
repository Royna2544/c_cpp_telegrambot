packages = [
    'libxml2',
    'libwebp',
    'opencv',
    'sqlite',
    'fmt',
    'cmake',
    'toolchain',
    'boost',
    'libgit2'
]

mappings = {
    'ucrt64': 'mingw-w64-ucrt-x86_64',
    'mingw32': 'mingw-w64-i686',
    'mingw64': 'mingw-w64-x86_64',
    'clang': 'mingw-w64-clang-x86_64',
}

def main():
    selected = input(f'Select a runtime. ({'/'.join([l for l in mappings.keys()])}): ')
    if selected not in mappings:
        print('Invalid runtime selected.')
        return
    print(f'Command is:\npacman -S {' '.join([f'{mappings[selected]}-{i}' for i in packages])}')

if __name__ == "__main__":
    main()