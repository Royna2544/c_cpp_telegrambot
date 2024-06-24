import sys

"""
Just a utility script to support dynamic list of commands compiled into.
"""

if len(sys.argv) < 3:
    print('Usage: %s gen_decl|gen_ptr|get_cmd_filenames|get_cmd_commands args...' % sys.argv[0], file=sys.stderr)
    sys.exit(1)

command = sys.argv[1]
libs = sys.argv[2:]

def parse_command_list(file):
    filenames = []
    commands = []
    with open(file, 'r') as f:
        for line in f.readlines():
            if line.startswith('#'):
                continue
            if line.strip() == '':
                continue
            if len(line.split('[')) > 2 or len(line.split(']')) > 2:
                print('Invalid line "%s", Two options in one line:' % line, file=sys.stderr)
                continue
            command = line.split('[')[0].strip()
            if command in commands:
                print('Duplicate command: %s' % command, file=sys.stderr)
                continue
            commands.append(command)
            if '[' and ']' in line:
                opt = line.split('[')[1].split(']')[0].strip()
                # Parse options
                if opt.startswith('!'):
                    # NOT platform declaration
                    match line[1:]:
                        case 'win32':
                            if sys.platform in ['win32', 'cygwin', 'msys']:
                                print('Ignore command: %s (Not Win32)' % command)
                        # TODO: Add more platforms
                elif opt.startswith('infile'):
                    if opt.find(' ') == -1:
                        print('Invalid option: %s' % opt, file=sys.stderr)
                        continue
                    str = opt.split(' ')[1]
                    if str not in filenames:
                        filenames.append(str)
                    continue
                else:
                    print('Invalid option: %s' % opt, file=sys.stderr)
                    continue
            filenames.append(command)
    return filenames, commands

if command == 'gen_decl':
    """ Generate command module loader functions' forward declarations. """
    decls = ''
    for lib in libs:
        decls += f"extern void loadcmd_{lib}(CommandModule &cmd);\n"
    print(decls)
elif command == 'gen_ptr':
    """ Generate command module loader functions' pointers. """
    ptrs = ''   
    for lib in libs:
        ptrs += ' ' * 8 + f"&loadcmd_{lib},\n"
    print(ptrs)
elif command == 'get_cmd_filenames':
    """ Get list of command module filenames. """
    filenames, _ = parse_command_list(sys.argv[2])
    for filename in filenames:
        print(filename, end=';')
elif command == 'get_cmd_names':
    """ Get list of command module name lists. """
    _, commands = parse_command_list(sys.argv[2])
    for command in commands:
        print(command, end=';')
else:
    print(f"Unknown command: {command}", file=sys.stderr)
    sys.exit(1)