#!/usr/bin/env python3
"""
Karma struct refactoring script
Automatically updates struct field references to use nested struct organization
"""

import re
import sys
from pathlib import Path

# Define the field mappings from old flat structure to new nested structure
FIELD_MAPPINGS = {
    # Buffer management group
    'buf': 'buffer.buf',
    'buf_temp': 'buffer.buf_temp',
    'bufname': 'buffer.bufname',  
    'bufname_temp': 'buffer.bufname_temp',
    'bframes': 'buffer.bframes',
    'bchans': 'buffer.bchans',
    'ochans': 'buffer.ochans',
    'nchans': 'buffer.nchans',
    
    # Timing and sample rate group
    'ssr': 'timing.ssr',
    'bsr': 'timing.bsr',
    'bmsr': 'timing.bmsr',
    'srscale': 'timing.srscale',
    'vs': 'timing.vs',
    'vsnorm': 'timing.vsnorm',
    'bvsnorm': 'timing.bvsnorm',
    'playhead': 'timing.playhead',
    'maxhead': 'timing.maxhead',
    'jumphead': 'timing.jumphead',
    'recordhead': 'timing.recordhead',
    'selstart': 'timing.selstart',
    'selection': 'timing.selection',
    
    # Audio processing group
    'o1prev': 'audio.o1prev',
    'o2prev': 'audio.o2prev',
    'o3prev': 'audio.o3prev',
    'o4prev': 'audio.o4prev',
    'o1dif': 'audio.o1dif',
    'o2dif': 'audio.o2dif',
    'o3dif': 'audio.o3dif',
    'o4dif': 'audio.o4dif',
    'writeval1': 'audio.writeval1',
    'writeval2': 'audio.writeval2',
    'writeval3': 'audio.writeval3',
    'writeval4': 'audio.writeval4',
    'overdubamp': 'audio.overdubamp',
    'overdubprev': 'audio.overdubprev',
    'interpflag': 'audio.interpflag',
    'pokesteps': 'audio.pokesteps',
    
    # Loop boundary group
    'minloop': 'loop.minloop',
    'maxloop': 'loop.maxloop',
    'startloop': 'loop.startloop',
    'endloop': 'loop.endloop',
    'initiallow': 'loop.initiallow',
    'initialhigh': 'loop.initialhigh',
    
    # Fade and ramp control group
    'recordfade': 'fade.recordfade',
    'playfade': 'fade.playfade',
    'globalramp': 'fade.globalramp',
    'snrramp': 'fade.snrramp',
    'snrfade': 'fade.snrfade',
    'snrtype': 'fade.snrtype',
    'playfadeflag': 'fade.playfadeflag',
    'recfadeflag': 'fade.recfadeflag',
    
    # State and control group
    'statecontrol': 'state.statecontrol',
    'statehuman': 'state.statehuman',
    'recendmark': 'state.recendmark',
    'directionorig': 'state.directionorig',
    'directionprev': 'state.directionprev',
    'stopallowed': 'state.stopallowed',
    'go': 'state.go',
    'record': 'state.record',
    'recordprev': 'state.recordprev',
    'loopdetermine': 'state.loopdetermine',
    'alternateflag': 'state.alternateflag',
    'append': 'state.append',
    'triginit': 'state.triginit',
    'wrapflag': 'state.wrapflag',
    'jumpflag': 'state.jumpflag',
    'recordinit': 'state.recordinit',
    'initinit': 'state.initinit',
    'initskip': 'state.initskip',
    'buf_modified': 'state.buf_modified',
    'clockgo': 'state.clockgo'
}

def refactor_struct_fields(content):
    """
    Refactor struct field access patterns in the given content
    """
    # Track statistics
    stats = {}
    for group in ['buffer', 'timing', 'audio', 'loop', 'fade', 'state']:
        stats[group] = 0
    
    modified_content = content
    
    # Process each field mapping
    for old_field, new_field in FIELD_MAPPINGS.items():
        group = new_field.split('.')[0]
        
        # Pattern to match struct field access: x->field or (*x).field or similar
        # This handles various pointer access patterns
        patterns = [
            rf'\b(\w+)->({old_field})\b',           # x->field
            rf'\b\(\*(\w+)\)\.({old_field})\b',     # (*x).field  
            rf'\b(\w+)\.({old_field})\b'            # x.field (direct access)
        ]
        
        for pattern in patterns:
            def replace_func(match):
                prefix = match.group(1)
                field = match.group(2)
                
                # Count the replacement
                stats[group] += 1
                
                # Return the replacement based on the access pattern
                if '->' in match.group(0):
                    return f"{prefix}->{new_field}"
                elif '(*' in match.group(0):
                    return f"(*{prefix}).{new_field}"
                else:
                    return f"{prefix}.{new_field}"
            
            modified_content = re.sub(pattern, replace_func, modified_content)
    
    return modified_content, stats

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 refactor_struct.py <karma~.c>")
        sys.exit(1)
    
    file_path = Path(sys.argv[1])
    
    if not file_path.exists():
        print(f"Error: File {file_path} not found")
        sys.exit(1)
    
    print(f"Refactoring struct fields in {file_path}...")
    
    # Read the file
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)
    
    # Apply refactoring
    modified_content, stats = refactor_struct_fields(content)
    
    # Create backup
    backup_path = file_path.with_suffix('.c.backup')
    try:
        with open(backup_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Backup created: {backup_path}")
    except Exception as e:
        print(f"Warning: Could not create backup: {e}")
    
    # Write the modified file
    try:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(modified_content)
    except Exception as e:
        print(f"Error writing file: {e}")
        sys.exit(1)
    
    # Print statistics
    print("\nRefactoring completed successfully!")
    print("Field reference updates by group:")
    total_updates = 0
    for group, count in stats.items():
        if count > 0:
            print(f"  {group}: {count} references updated")
            total_updates += count
    
    print(f"\nTotal updates: {total_updates}")
    
    if total_updates == 0:
        print("No field references found to update.")
    else:
        print(f"File {file_path} has been updated with nested struct references.")

if __name__ == "__main__":
    main()