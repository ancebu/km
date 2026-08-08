
Import("env")

import re

def process_template(template_file, output_file):
    with open(template_file, 'r') as f_in, open(output_file, 'w') as f_out:
        for line in f_in:
            # Skip lines starting with cmakedefine
            if line.strip().startswith('#cmakedefine'):
                continue

            # Skip lines containing patterns like SOMETHING @NAME@
            if re.search(r'@(\w+)@', line):
                continue

            f_out.write(line)

TEMPLATE_FILE = "src/catch2/catch_user_config.hpp.in"
OUTPUT_FILE = "src/catch2/catch_user_config.hpp"

process_template(TEMPLATE_FILE, OUTPUT_FILE)
print(f"Processed {TEMPLATE_FILE} and wrote result to {OUTPUT_FILE}")
