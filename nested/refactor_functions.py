#!/usr/bin/env python3
"""
Karma function signature refactoring script
Refactors kh_ helper functions to use struct pointers instead of many individual parameters
"""

import re
import sys
from pathlib import Path

# Define function refactorings - functions that would benefit most from struct pointer parameters
FUNCTION_REFACTORS = {
    'kh_process_recording_cleanup': {
        'old_params': [
            't_bool record', 'double globalramp', 'long frames', 'float *b', 'long pchans',
            'double accuratehead', 'long *recordhead', 'char direction', 'long *recordfade',
            'char *recfadeflag', 'double *snrfade', 't_bool use_ease_on', 'double ease_pos'
        ],
        'new_params': [
            't_karma *x', 'float *b', 'long frames', 'long pchans',
            'double accuratehead', 'char direction', 't_bool use_ease_on', 'double ease_pos'
        ]
    },
    
    'kh_process_loop_boundary': {
        'old_params': [
            'double *accuratehead', 'double speed', 'double srscale', 'char direction',
            'char directionorig', 'long frames', 'long maxloop', 'long minloop',
            'long setloopsize', 'long startloop', 'long endloop', 't_bool wrapflag',
            't_bool jumpflag', 't_bool record', 'double globalramp', 'float *b',
            'long pchans', 'long *recordhead', 'long *recordfade', 'char *recfadeflag',
            'double *snrfade'
        ],
        'new_params': [
            't_karma *x', 'double *accuratehead', 'double speed', 'char direction',
            'long frames', 'long setloopsize', 'float *b', 'long pchans'
        ]
    },
    
    'kh_process_loop_initialization': {
        'old_params': [
            't_bool triginit', 'char recendmark', 'char directionorig',
            'long *maxloop', 'long minloop', 'long maxhead', 'long frames',
            'double selstart', 'double selection', 'double *accuratehead',
            'long *startloop', 'long *endloop', 'long *setloopsize',
            't_bool *wrapflag', 'char direction', 'double globalramp',
            'float *b', 'long pchans', 'long recordhead', 't_bool record',
            't_bool jumpflag', 'double jumphead', 'double *snrfade',
            't_bool *append', 't_bool *alternateflag', 'char *recendmark_ptr'
        ],
        'new_params': [
            't_karma *x', 'double *accuratehead', 'long *setloopsize',
            'char direction', 'long frames', 'float *b', 'long pchans'
        ]
    },
    
    'kh_process_initial_loop_creation': {
        'old_params': [
            't_bool go', 't_bool triginit', 't_bool jumpflag', 't_bool append',
            'double jumphead', 'long maxhead', 'long frames', 'char directionorig',
            'double *accuratehead', 'double *snrfade', 't_bool record',
            'double globalramp', 'float *b', 'long pchans', 'long *recordhead',
            'char direction', 'long *recordfade', 'char *recfadeflag'
        ],
        'new_params': [
            't_karma *x', 'double *accuratehead', 'long frames',
            'float *b', 'long pchans', 'char direction'
        ]
    },
    
    'kh_process_forward_jump_boundary': {
        'old_params': [
            'double *accuratehead', 'long maxloop', 'long setloopsize', 't_bool record',
            'double globalramp', 'long frames', 'float *b', 'long pchans', 'long *recordhead',
            'char direction', 'long *recordfade', 'char *recfadeflag', 'double *snrfade'
        ],
        'new_params': [
            't_karma *x', 'double *accuratehead', 'long setloopsize',
            'long frames', 'float *b', 'long pchans', 'char direction'
        ]
    },
    
    'kh_process_reverse_jump_boundary': {
        'old_params': [
            'double *accuratehead', 'long frames', 'long setloopsize', 'long maxloop',
            't_bool record', 'double globalramp', 'float *b', 'long pchans', 'long *recordhead',
            'char direction', 'long *recordfade', 'char *recfadeflag', 'double *snrfade'
        ],
        'new_params': [
            't_karma *x', 'double *accuratehead', 'long frames', 'long setloopsize',
            'float *b', 'long pchans', 'char direction'
        ]
    }
}

def refactor_function_signatures(content):
    """
    Refactor function signatures and their forward declarations
    """
    modified_content = content
    stats = {'declarations_updated': 0, 'definitions_updated': 0}
    
    for func_name, refactor_info in FUNCTION_REFACTORS.items():
        old_params = refactor_info['old_params']
        new_params = refactor_info['new_params']
        
        # Create parameter strings
        old_param_str = ', '.join(old_params)
        new_param_str = ', '.join(new_params)
        
        # Update forward declarations
        decl_pattern = rf'static inline [^(]+{func_name}\s*\([^)]+\);'
        def replace_decl(match):
            stats['declarations_updated'] += 1
            return f"static inline void {func_name}({new_param_str});"
        
        modified_content = re.sub(decl_pattern, replace_decl, modified_content, flags=re.MULTILINE)
        
        # Update function definitions
        def_pattern = rf'static inline [^(]+{func_name}\s*\([^)]+\)\s*\{{'
        def replace_def(match):
            stats['definitions_updated'] += 1
            return f"static inline void {func_name}({new_param_str})\n{{"
        
        modified_content = re.sub(def_pattern, replace_def, modified_content, flags=re.MULTILINE | re.DOTALL)
    
    return modified_content, stats

def update_function_calls(content):
    """
    Update function calls to match new signatures - this is a simplified version
    that adds a comment marker where manual updates are needed
    """
    modified_content = content
    stats = {'call_sites_marked': 0}
    
    for func_name in FUNCTION_REFACTORS.keys():
        # Find function calls and mark them for manual update
        call_pattern = rf'{func_name}\s*\([^)]+\);'
        def mark_call(match):
            stats['call_sites_marked'] += 1
            return f"// TODO: Update call signature\n    {match.group(0)}"
        
        modified_content = re.sub(call_pattern, mark_call, modified_content, flags=re.MULTILINE)
    
    return modified_content, stats

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 refactor_functions.py <karma~.c>")
        sys.exit(1)
    
    file_path = Path(sys.argv[1])
    
    if not file_path.exists():
        print(f"Error: File {file_path} not found")
        sys.exit(1)
    
    print(f"Refactoring function signatures in {file_path}...")
    
    # Read the file
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)
    
    # Apply function signature refactoring
    modified_content, sig_stats = refactor_function_signatures(content)
    
    # Mark function calls that need updating
    modified_content, call_stats = update_function_calls(modified_content)
    
    # Create backup
    backup_path = file_path.with_suffix('.c.backup2')
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
    print("\nFunction refactoring completed!")
    print(f"Function declarations updated: {sig_stats['declarations_updated']}")
    print(f"Function definitions updated: {sig_stats['definitions_updated']}")
    print(f"Function call sites marked for manual update: {call_stats['call_sites_marked']}")
    
    print(f"\nRefactored functions:")
    for func_name, refactor_info in FUNCTION_REFACTORS.items():
        old_count = len(refactor_info['old_params'])
        new_count = len(refactor_info['new_params'])
        print(f"  {func_name}: {old_count} → {new_count} parameters ({old_count - new_count} reduction)")
    
    print("\nNote: Function call sites have been marked with TODO comments.")
    print("Manual review and updating of the function calls is recommended.")

if __name__ == "__main__":
    main()