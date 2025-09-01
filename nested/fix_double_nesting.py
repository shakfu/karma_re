#!/usr/bin/env python3
"""
Fix double-nested struct references that were created during refactoring
"""

import re
import sys
from pathlib import Path

def fix_double_nesting(content):
    """
    Fix patterns like x->buffer.buffer.field to x->buffer.field
    """
    # Pattern to match double nesting: x->group.group.field
    patterns = [
        r'(\w+)->buffer\.buffer\.(\w+)', r'\1->buffer.\2',
        r'(\w+)->timing\.timing\.(\w+)', r'\1->timing.\2',
        r'(\w+)->audio\.audio\.(\w+)', r'\1->audio.\2',
        r'(\w+)->loop\.loop\.(\w+)', r'\1->loop.\2',
        r'(\w+)->fade\.fade\.(\w+)', r'\1->fade.\2',
        r'(\w+)->state\.state\.(\w+)', r'\1->state.\2'
    ]
    
    modified_content = content
    total_fixes = 0
    
    for i in range(0, len(patterns), 2):
        pattern = patterns[i]
        replacement = patterns[i + 1]
        
        matches = len(re.findall(pattern, modified_content))
        if matches > 0:
            print(f"Fixing {matches} double-nested references: {pattern}")
            total_fixes += matches
            
        modified_content = re.sub(pattern, replacement, modified_content)
    
    return modified_content, total_fixes

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 fix_double_nesting.py <karma~.c>")
        sys.exit(1)
    
    file_path = Path(sys.argv[1])
    
    # Read the file
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Fix double nesting
    modified_content, total_fixes = fix_double_nesting(content)
    
    # Write the file
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(modified_content)
    
    print(f"Fixed {total_fixes} double-nested references")

if __name__ == "__main__":
    main()