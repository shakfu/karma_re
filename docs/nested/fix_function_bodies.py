#!/usr/bin/env python3
"""
Fix function bodies to use struct member access instead of old parameters
"""

import re
import sys
from pathlib import Path

def fix_function_body_references(content):
    """
    Update function bodies to use x->group.field instead of parameter names
    """
    
    # Define the mappings for function bodies
    replacements = [
        # Parameters that now come from structs
        (r'\brecord\b(?!\w)', 'x->state.record'),
        (r'\bglobalramp\b(?!\w)', 'x->fade.globalramp'),
        (r'\*recordhead\b(?!\w)', 'x->timing.recordhead'),
        (r'\brecordhead\b(?!\w)', '&x->timing.recordhead'),
        (r'\*recordfade\b(?!\w)', 'x->fade.recordfade'),
        (r'\brecordfade\b(?!\w)', '&x->fade.recordfade'),
        (r'\*recfadeflag\b(?!\w)', 'x->fade.recfadeflag'),
        (r'\brecfadeflag\b(?!\w)', '&x->fade.recfadeflag'),
        (r'\*snrfade\b(?!\w)', 'x->fade.snrfade'),
        (r'\bsnrfade\b(?!\w)', '&x->fade.snrfade'),
        (r'\bmaxloop\b(?!\w)', 'x->loop.maxloop'),
        (r'\bminloop\b(?!\w)', 'x->loop.minloop'),
        (r'\bstartloop\b(?!\w)', 'x->loop.startloop'),
        (r'\bendloop\b(?!\w)', 'x->loop.endloop'),
        (r'\bwrapflag\b(?!\w)', 'x->state.wrapflag'),
        (r'\bjumpflag\b(?!\w)', 'x->state.jumpflag'),
        (r'\bdirectionorig\b(?!\w)', 'x->state.directionorig'),
        (r'\bsrscale\b(?!\w)', 'x->timing.srscale'),
        (r'\btriginit\b(?!\w)', 'x->state.triginit'),
        (r'\bloopdetermine\b(?!\w)', 'x->state.loopdetermine'),
        (r'\bappend\b(?!\w)', 'x->state.append'),
        (r'\balternateflag\b(?!\w)', 'x->state.alternateflag'),
        (r'\brecendmark\b(?!\w)', 'x->state.recendmark'),
        (r'\bgo\b(?!\w)', 'x->state.go'),
        (r'\bmaxhead\b(?!\w)', 'x->timing.maxhead'),
        (r'\bselstart\b(?!\w)', 'x->timing.selstart'),
        (r'\bselection\b(?!\w)', 'x->timing.selection'),
        (r'\bjumphead\b(?!\w)', 'x->timing.jumphead'),
        (r'\bplayfade\b(?!\w)', '&x->fade.playfade'),
        (r'\*playfade\b(?!\w)', 'x->fade.playfade'),
        (r'\bplayfadeflag\b(?!\w)', '&x->fade.playfadeflag'),
        (r'\*playfadeflag\b(?!\w)', 'x->fade.playfadeflag'),
    ]
    
    modified_content = content
    total_fixes = 0
    
    for pattern, replacement in replacements:
        matches = len(re.findall(pattern, modified_content))
        if matches > 0:
            print(f"Fixing {matches} references: {pattern} -> {replacement}")
            total_fixes += matches
            modified_content = re.sub(pattern, replacement, modified_content)
    
    return modified_content, total_fixes

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 fix_function_bodies.py <karma~.c>")
        sys.exit(1)
    
    file_path = Path(sys.argv[1])
    
    # Read the file
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Fix function body references
    modified_content, total_fixes = fix_function_body_references(content)
    
    # Write the file
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(modified_content)
    
    print(f"Fixed {total_fixes} function body references")

if __name__ == "__main__":
    main()