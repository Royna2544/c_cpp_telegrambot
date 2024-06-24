import sys
import re

"""
Just a utility script to support dynamic list of commands compiled into.
"""

if len(sys.argv) < 3:
    print('Usage: %s gen_decl|gen_ptr|get_cmd_filenames|get_cmd_commands args...' % sys.argv[0], file=sys.stderr)
    sys.exit(1)

command = sys.argv[1]
libs = sys.argv[2:]
cmdlist_regex = re.compile(r'^([a-zA-Z]+)(?: \[([^\]]+)\])?$')

def parse_command_list(file):
    filenames = []
    commands = []
    with open(file, 'r') as f:
        for line in f.readlines():
            if line.startswith('#') or line.strip() == '':
                continue
            regexOut = cmdlist_regex.match(line)
            if not regexOut:
                print('Invalid line "%s", doesn\'t match regex' % line, file=sys.stderr)
                continue
            command = regexOut.group(1)
            options = regexOut.group(2)
            
            if command in commands:
                print('Duplicate command: %s' % command, file=sys.stderr)
                continue
            
            isInSperateFile = True
            # Parse options
            if options:
                # Split the parameters by comma and process each key=value pair
                for option, value in dict(param.split('=') for param in options.split(',')).items():
                    match option:
                        case 'target':
                            match value:
                                case '!win32':
                                    if sys.platform in ['win32', 'cygwin', 'msys']:
                                        print('Ignore command: %s (Not Win32)' % command)
                                # TODO: Add more platforms
                        case 'infile':
                            if value not in filenames:
                                filenames.append(value)
                            isInSperateFile = False
                            continue
                        case _:
                            print('Invalid option: %s' % option, file=sys.stderr)
                            continue
            if isInSperateFile:
                filenames.append(command)
            commands.append(command)
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