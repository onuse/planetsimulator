# Claude Tools Integration Strategy
## Ensuring Persistent Tool Usage Across Context Resets

## The Problem
- Claude loses context during auto-compact events
- Custom tool instructions get forgotten
- Reverts to using default (broken) tools
- Need persistent, discoverable instruction mechanism

## Solution: Multi-Layer Persistence Strategy

### 1. Project-Level Configuration File (.claude/config.yml)
```yaml
# .claude/config.yml - ALWAYS checked by Claude
tools:
  override_defaults: true
  
  edit:
    command: "python3 ${CLAUDE_TOOLS_PATH}/claude_tools.py"
    prefer_over: ["Edit", "Write"]
    
  rename:
    command: "python3 ${CLAUDE_TOOLS_PATH}/claude_tools.py rename"
    use_for: ["refactoring", "symbol renaming"]
    
  safe_move:
    command: "python3 ${CLAUDE_TOOLS_PATH}/claude_tools.py move"
    use_for: ["file reorganization"]

instructions:
  - "ALWAYS check for .claude/config.yml before using any edit tools"
  - "If Edit tool fails with 'modification' error, use claude_tools.py instead"
  - "For any refactoring, use claude_tools.py rename/move commands"
```

### 2. Tool Discovery Script (claude_init.sh)
```bash
#!/bin/bash
# claude_init.sh - Run this at session start or after context reset

echo "=== Claude Tools Available ==="
echo "Detected custom tools in: $(pwd)/.claude/"
echo ""
echo "IMPORTANT: Default Edit/Write tools are BROKEN in WSL"
echo "Use these instead:"
echo "  - python3 claude_tools.py edit <file> <old> <new>"
echo "  - python3 claude_tools.py rename <old> <new> --execute"
echo "  - python3 claude_tools.py move <from> <to>"
echo ""
echo "These tools bypass modification detection issues!"

# Check if in git repo
if [ -d .git ]; then
    echo ""
    echo "Git hooks detected. Tools will auto-backup before changes."
fi

# Export path for easy access
export CLAUDE_TOOLS_PATH="$(pwd)/.claude"
echo "Tools path exported: \$CLAUDE_TOOLS_PATH"
```

### 3. README.claude.md - Context-Resistant Instructions
```markdown
# CRITICAL: Tool Usage Instructions for Claude

## ⚠️ IMPORTANT: Default Edit Tool is Broken in WSL
The default Edit tool has false modification detection. 
**ALWAYS USE** `claude_tools.py` instead.

## Quick Reference (Copy-Paste Commands)

### When Edit Tool Fails
```bash
# Instead of Edit tool, use:
python3 .claude/claude_tools.py edit <file> "<old_text>" "<new_text>"

# For multiple edits:
python3 .claude/claude_tools.py bulk-edit changes.json
```

### For Refactoring
```bash
# Rename across project
python3 .claude/claude_tools.py rename "oldName" "newName" --execute

# Move file and update includes
python3 .claude/claude_tools.py move "old/path.cpp" "new/path.cpp"
```

## Auto-Detection Pattern
If you see this error:
```
Error: File has been modified since read
```
Immediately switch to:
```bash
python3 .claude/claude_tools.py edit <file> "<old>" "<new>"
```
```

### 4. Git Hooks for Persistence (.git/hooks/pre-commit)
```bash
#!/bin/bash
# Remind Claude about custom tools on every commit

if [ -f .claude/config.yml ]; then
    echo "📌 Reminder: Custom Claude tools active in .claude/"
    echo "   Use 'claude_tools.py' for editing (not default Edit tool)"
fi
```

### 5. Tool Wrapper with Auto-Detection
```python
# .claude/smart_edit.py
#!/usr/bin/env python3
"""
Smart edit wrapper that auto-detects and handles edit failures
"""

import subprocess
import sys
import os

def smart_edit(file_path, old_text, new_text):
    """
    Try default edit first, fall back to claude_tools if it fails
    """
    # First, check if claude_tools.py exists
    claude_tools_path = ".claude/claude_tools.py"
    
    if os.path.exists(claude_tools_path):
        # Use claude_tools directly
        print("Using Claude Tools (bypassing broken Edit)...")
        result = subprocess.run([
            "python3", claude_tools_path, "edit", 
            file_path, old_text, new_text
        ], capture_output=True, text=True)
        
        if result.returncode == 0:
            print("✅ Edit successful with Claude Tools")
        else:
            print(f"❌ Edit failed: {result.stderr}")
        return result.returncode
    else:
        print("⚠️ Claude Tools not found. Install from:")
        print("   https://github.com/yourusername/claude-tools")
        return 1

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: smart_edit.py <file> <old> <new>")
        sys.exit(1)
    
    sys.exit(smart_edit(sys.argv[1], sys.argv[2], sys.argv[3]))
```

### 6. Environment Variable Approach
```bash
# .envrc or .bashrc
export CLAUDE_USE_CUSTOM_TOOLS=true
export CLAUDE_TOOLS_PATH="$HOME/.claude-tools"
export CLAUDE_EDIT_COMMAND="python3 $CLAUDE_TOOLS_PATH/claude_tools.py edit"

# Claude can check these on every operation
if [ "$CLAUDE_USE_CUSTOM_TOOLS" = "true" ]; then
    alias edit="$CLAUDE_EDIT_COMMAND"
fi
```

### 7. Inline Documentation Pattern
```python
"""
FILE: any_file.py
CLAUDE_INSTRUCTIONS: Use claude_tools.py for ALL edits in this project
TOOLS_PATH: .claude/claude_tools.py
FALLBACK: If Edit fails, use: python3 .claude/claude_tools.py edit
"""
```

## Implementation Priority

### Phase 1: Immediate (Today)
1. Create `.claude/` directory structure
2. Add `claude_tools.py` and `config.yml`
3. Add README.claude.md to project root

### Phase 2: GitHub Repo Setup (This Week)
```bash
# Create new repo: github.com/yourusername/claude-tools

claude-tools/
├── README.md           # For humans
├── README.claude.md    # For Claude (context-resistant)
├── install.sh          # One-line installer
├── claude_tools.py     # Main tool
├── config.template.yml # Template config
└── examples/
    ├── basic_edit.md
    ├── refactoring.md
    └── transactions.md
```

### Phase 3: One-Line Installer
```bash
# Install to any project:
curl -sSL https://github.com/yourusername/claude-tools/raw/main/install.sh | bash

# This will:
# 1. Create .claude/ directory
# 2. Download claude_tools.py
# 3. Add config.yml
# 4. Update .gitignore
# 5. Show success message with instructions
```

## Context-Resistant Instruction Patterns

### Pattern 1: Error Message Triggers
```python
# In claude_tools.py, add helpful error messages:
if "modification" in str(error):
    print("💡 TIP: This error means the default Edit tool is broken.")
    print("   Use: python3 .claude/claude_tools.py edit <file> <old> <new>")
```

### Pattern 2: Command Aliases
```bash
# .claude/aliases.sh - source this after context reset
alias cedit='python3 .claude/claude_tools.py edit'
alias crename='python3 .claude/claude_tools.py rename'
alias cmove='python3 .claude/claude_tools.py move'

echo "Claude tool aliases loaded: cedit, crename, cmove"
```

### Pattern 3: Diagnostic Check
```python
# .claude/check_tools.py
#!/usr/bin/env python3
"""Run this to verify Claude tools are working"""

import os
import sys

print("🔍 Checking Claude Tools Setup...")

checks = [
    ("claude_tools.py exists", os.path.exists(".claude/claude_tools.py")),
    ("config.yml exists", os.path.exists(".claude/config.yml")),
    ("README.claude.md exists", os.path.exists("README.claude.md")),
]

for check, result in checks:
    print(f"  {'✅' if result else '❌'} {check}")

if not all(r for _, r in checks):
    print("\n⚠️  Setup incomplete! Run: curl -sSL .../install.sh | bash")
else:
    print("\n✅ Claude Tools ready! If Edit fails, use:")
    print("   python3 .claude/claude_tools.py edit <file> <old> <new>")
```

## Key Strategies for Persistence

1. **Multiple Redundant Indicators**
   - Config files
   - README files  
   - Error messages
   - Environment variables
   - Git hooks

2. **Automatic Fallback**
   - Smart wrappers that detect failures
   - Helpful error messages with exact commands
   - Auto-switch to working tools

3. **Easy Recovery**
   - One-line tool check: `python3 .claude/check_tools.py`
   - One-line reinstall: `curl ... | bash`
   - Clear command examples in multiple places

4. **Context Clues**
   - File naming (.claude/* makes it obvious)
   - Error messages that explain the fix
   - Comments in code files

## Testing Context Persistence

```bash
# Simulate context loss
echo "TEST: Pretending context was reset..."

# Check if Claude remembers to use custom tools
if [ -f .claude/config.yml ]; then
    echo "✅ Config found - Claude should detect custom tools"
fi

if [ -f README.claude.md ]; then
    echo "✅ Instructions found - Claude can recover"
fi

# Try an edit
python3 .claude/claude_tools.py edit test.txt "old" "new" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✅ Tools working even after context reset"
fi
```

## Summary

The key to persistent tool usage across context resets is **redundancy** and **discoverability**. By placing instructions in multiple locations (config files, READMEs, error messages, environment variables), we ensure Claude can always find and use the correct tools, even after complete context loss.

The most effective approach:
1. **Visible project structure** (`.claude/` directory)
2. **Self-documenting errors** (tell Claude what to do when things fail)
3. **One-line recovery** (simple commands to get back on track)
4. **Multiple instruction sources** (config, README, inline comments)

This way, even if Claude forgets everything, the project itself reminds it how to work correctly!