# html2c.py: Convert HTML to C string array for embedding
import sys
import pathlib

def html_to_c_array(infile, outfile):
    with open(infile, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    with open(outfile, 'w', encoding='utf-8') as f:
        for line in lines:
            # Escape backslashes and double quotes, add \n
            line = line.rstrip('\n').replace('\\', '\\\\').replace('"', '\\"')
            f.write(f'"{line}\\n"\n')

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python html2c.py input.html output.inc")
        sys.exit(1)
    html_to_c_array(sys.argv[1], sys.argv[2])
