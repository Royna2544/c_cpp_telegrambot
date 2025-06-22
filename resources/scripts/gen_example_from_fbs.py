import json
import re

TYPE_EXAMPLES = {
    "int": 123,
    "uint": 123,
    "float": 1.23,
    "double": 3.1415,
    "bool": True,
    "string": "example",
    "byte": 42,
    "ubyte": 255,
    "short": 32000,
    "ushort": 65000,
    "long": 1234567890,
    "ulong": 1234567890123,
}

def parse_schema(schema_text):
    tables = {}
    enums = {}
    current_table = None
    current_enum = None
    inside_table = False
    inside_enum = False

    for line in schema_text.splitlines():
        line = line.strip()
        if line.startswith("//") or not line:
            continue

        if line.startswith("table "):
            current_table = re.findall(r'table\s+(\w+)', line)[0]
            tables[current_table] = {}
            inside_table = True
            continue

        if line.startswith("enum "):
            current_enum = re.findall(r'enum\s+(\w+)', line)[0]
            enums[current_enum] = []
            inside_enum = True
            continue

        if inside_table and line == "}":
            inside_table = False
            current_table = None
            continue

        if inside_enum and line == "}":
            inside_enum = False
            current_enum = None
            continue

        if inside_table and current_table:
            match = re.match(r'(\w+):\s*([\w<>]+)', line)
            if match:
                field, ftype = match.groups()
                tables[current_table][field] = ftype

        if inside_enum and current_enum:
            enum_val = re.match(r'(\w+)', line)
            if enum_val:
                enums[current_enum].append(enum_val.group(1))

    return tables, enums

def generate_example_value(ftype, enums):
    is_array = False
    if ftype.endswith("]"):
        is_array = True
        ftype = ftype.lstrip("[").rstrip("]")

    if ftype in TYPE_EXAMPLES:
        value = TYPE_EXAMPLES[ftype]
    elif ftype in enums:
        value = enums[ftype][0]  # Use first enum value
    else:
        value = generate_example_object(ftype, enums)

    return [value] if is_array else value

def generate_example_object(table_name, enums):
    if table_name not in parsed_tables:
        return {}

    result = {}
    for field, ftype in parsed_tables[table_name].items():
        result[field] = generate_example_value(ftype, enums)
    return result

def generate_json_from_schema(schema_file_path, root_table=None):
    with open(schema_file_path, 'r') as f:
        schema_text = f.read()

    global parsed_tables
    parsed_tables, parsed_enums = parse_schema(schema_text)

    if not root_table:
        root_table = list(parsed_tables.keys())[0]  # Default to first table

    example_json = generate_example_object(root_table, parsed_enums)
    return json.dumps(example_json, indent=2)


# Example usage:
if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: python fbs_to_json_example.py schema.fbs [RootTableName]")
    else:
        schema_file = sys.argv[1]
        root = sys.argv[2] if len(sys.argv) > 2 else None
        example = generate_json_from_schema(schema_file, root)
        print(example)
