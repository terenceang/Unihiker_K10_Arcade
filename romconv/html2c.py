# html2c.py: Convert HTML to C string array for embedding with SSOT injection
import sys
import os
import re

def extract_bitmasks(header_file):
    masks = []
    if not os.path.exists(header_file):
        return ""
    
    with open(header_file, 'r') as f:
        content = f.read()
        # Find K10_BUTTON_... = 0x.. patterns
        matches = re.findall(r'(K10_BUTTON_\w+)\s*=\s*(0x[0-9a-fA-F]+)', content)
        for name, value in matches:
            masks.append(f"const {name} = {value};")
    
    return "\n    ".join(masks)

def html_to_c_array(infile, outfile):
    # Determine header path for SSOT
    header_path = os.path.join(os.path.dirname(infile), 'k10_input.h')
    bitmasks = extract_bitmasks(header_path)

    with open(infile, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Inject bitmasks
    if "/* BITMASK_PLACEHOLDER */" in content:
        content = content.replace("/* BITMASK_PLACEHOLDER */", bitmasks)
    
    lines = content.splitlines()
    
    with open(outfile, 'w', encoding='utf-8') as f:
        for line in lines:
            # Escape backslashes and double quotes, add \n
            line = line.replace('\\', '\\\\').replace('"', '\\"')
            f.write(f'"{line}\\n"\n')

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python html2c.py input.html output.inc")
        sys.exit(1)
    html_to_c_array(sys.argv[1], sys.argv[2])
