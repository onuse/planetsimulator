# Claude Development Tools Suite - Design Proposal

## Problem Statement
Current AI editing tools suffer from:
- False modification detection (as we just experienced)
- Limited atomic operations (can't rename across files)
- No refactoring support
- Poor multi-file operations
- No transaction/rollback capability
- WSL/Windows path issues

## Proposed Tool Suite

### 1. Core Editing Tools

#### `safe_edit`
```python
safe_edit(file_path, changes=[
    {"old": "text", "new": "text", "occurrence": 1},  # which occurrence
    {"old": "text", "new": "text", "all": true}       # all occurrences
])
```
- No modification checks - just do it
- Support multiple edits in one atomic operation
- Return success/failure with line numbers changed

#### `patch_apply`
```python
patch_apply(file_path, patch_string)  # Apply unified diff format
patch_apply(file_path, line_edits=[   # Or structured edits
    {"line": 25, "content": "new line content"},
    {"delete_lines": [30, 35]},
    {"insert_after": 40, "content": ["line1", "line2"]}
])
```

#### `smart_insert`
```python
smart_insert(file_path, 
    after_pattern="class MyClass:",  # Insert after this pattern
    content="    def new_method(self):\n        pass",
    indent_level="auto")  # Auto-detect indentation
```

### 2. Refactoring Tools

#### `rename_symbol`
```python
rename_symbol(
    old_name="generateUnifiedSphere",
    new_name="generateSphereMeshWithLOD",
    scope="project",  # or "file", "directory"
    file_types=[".cpp", ".hpp"],
    preview=True  # Show what will change first
)
```
- Handles all references across codebase
- Respects language semantics (not just string replace)

#### `extract_function`
```python
extract_function(
    file_path="main.cpp",
    start_line=50,
    end_line=75,
    function_name="calculateLODLevel",
    target_file="lod_utils.cpp"  # Optional: move to different file
)
```

#### `move_definition`
```python
move_definition(
    symbol="generateUnifiedSphere",
    from_file="vulkan_renderer.cpp",
    to_file="sphere_generation.cpp",
    update_includes=True
)
```

### 3. Multi-File Operations

#### `bulk_edit`
```python
bulk_edit(operations=[
    {"file": "a.cpp", "old": "x", "new": "y"},
    {"file": "b.hpp", "old": "x", "new": "y"},
], 
transaction=True)  # All or nothing
```

#### `find_replace_all`
```python
find_replace_all(
    pattern="regex_pattern",
    replacement="new_text",
    file_pattern="**/*.cpp",
    exclude_dirs=["build", "external"],
    dry_run=True  # Preview changes
)
```

### 4. Safe File Operations

#### `safe_delete`
```python
safe_delete(
    file_path="old_file.cpp",
    check_references=True,  # Warn if file is referenced elsewhere
    backup=True  # Create .bak file
)
```

#### `safe_move`
```python
safe_move(
    from_path="src/old_location.cpp",
    to_path="src/new_location.cpp",
    update_includes=True,  # Fix #include statements
    update_cmake=True  # Update CMakeLists.txt
)
```

### 5. Code Analysis Tools

#### `find_usages`
```python
find_usages(
    symbol="generateUnifiedSphere",
    context_lines=2,
    group_by_file=True
)
# Returns: {"file.cpp": [{"line": 45, "context": "..."}]}
```

#### `dependency_graph`
```python
dependency_graph(
    file="vulkan_renderer.cpp",
    depth=2,  # How many levels of dependencies
    format="json"  # or "dot", "ascii"
)
```

### 6. Transaction Support

#### `edit_transaction`
```python
with edit_transaction() as tx:
    tx.edit("file1.cpp", old="x", new="y")
    tx.rename_symbol("oldName", "newName")
    tx.move_file("a.cpp", "b.cpp")
    # Automatically rolls back if any operation fails
```

### 7. WSL/Cross-Platform Support

#### `path_convert`
```python
path_convert("/mnt/c/Users/...", to="windows")  # Returns C:\Users\...
path_convert("C:\\Users\\...", to="wsl")  # Returns /mnt/c/Users/...
```

#### `platform_exec`
```python
platform_exec(
    command="cl.exe /c file.cpp",  # Windows command
    platform="windows",  # Execute in Windows context from WSL
    working_dir="/mnt/c/project"
)
```

## Implementation Architecture

### Core Principles
1. **No False Positives**: Direct file I/O, no modification checks
2. **Atomic Operations**: All changes succeed or all fail
3. **Language Aware**: Use tree-sitter or LSP for semantic understanding
4. **Fast**: Parallel operations where possible
5. **Safe**: Always preview destructive operations
6. **Trackable**: Log all operations for debugging

### Technology Stack
```python
# Core implementation
- Python 3.8+ (WSL native)
- tree-sitter (parsing for refactoring)
- ripgrep (fast searching)
- git (optional version control integration)

# Optional enhancements
- Language servers (pylsp, clangd, etc.)
- AST manipulation libraries per language
```

### Error Handling
```python
class ToolResult:
    success: bool
    changes_made: List[FileChange]
    errors: List[str]
    warnings: List[str]
    rollback_available: bool
    
    def undo(self):
        """Revert all changes made"""
        pass
```

## Usage Examples for Claude

### Example 1: Add Method to Class
```python
# Instead of complex string matching:
smart_insert(
    "renderer.cpp",
    after_pattern="class VulkanRenderer {",
    content="""
    void updateLOD(Camera* camera) {
        // implementation
    }
    """
)
```

### Example 2: Rename Across Project
```python
# Instead of multiple individual edits:
rename_symbol(
    old_name="generateUnifiedSphere",
    new_name="generateSphereLOD",
    scope="project",
    preview=True
)
# Shows: "Will update 15 references across 8 files"
# Then: confirm=True to execute
```

### Example 3: Safe Refactoring
```python
with edit_transaction() as tx:
    # Extract LOD logic to separate function
    tx.extract_function(
        file="sphere.cpp",
        start_line=35,
        end_line=60,
        function_name="calculateLODLevel"
    )
    # Move to utilities file
    tx.move_definition(
        "calculateLODLevel",
        to_file="lod_utils.cpp"
    )
    # Update all callers
    tx.add_include("sphere.cpp", "#include \"lod_utils.hpp\"")
```

## Benefits for AI Assistants

1. **Reduced Errors**: No false modification detection
2. **Higher-Level Operations**: Think in refactoring, not text editing
3. **Confidence**: Preview and transaction support
4. **Efficiency**: Batch operations reduce round trips
5. **Cross-Platform**: Works identically on Windows/Linux/Mac
6. **Semantic Understanding**: Not just text manipulation

## Minimal MVP Implementation

Start with these essential tools:
```python
# Phase 1 - Core (1-2 days)
- force_edit (no modification check)
- bulk_edit (multiple files)
- find_replace_all

# Phase 2 - Refactoring (2-3 days)  
- rename_symbol (basic version)
- safe_move (with include updates)

# Phase 3 - Transactions (1-2 days)
- edit_transaction context manager
- rollback capability
```

## Testing Strategy

```python
# Test harness with virtual file system
def test_rename_symbol():
    with TestFileSystem() as fs:
        fs.create_file("main.cpp", "void foo() { bar(); }")
        fs.create_file("other.cpp", "void bar() { return; }")
        
        rename_symbol("bar", "baz", scope="project")
        
        assert "baz()" in fs.read("main.cpp")
        assert "void baz()" in fs.read("other.cpp")
```

## Configuration

```yaml
# .claude-tools.yml
tools:
  safe_edit:
    backup: true
    encoding: utf-8
  rename_symbol:
    use_lsp: true
    languages:
      cpp: clangd
      python: pylsp
  paths:
    prefer: wsl  # or "windows"
```

## Open Questions

1. Should we integrate with git for automatic commits?
2. How much language-specific logic vs generic text manipulation?
3. Should tools auto-detect project type (CMake, npm, etc)?
4. Include AI-specific hints in tool responses?
5. Build as Python package, standalone binary, or both?

---

This tool suite would dramatically improve AI-assisted development by providing robust, high-level operations that map directly to developer intent rather than low-level text manipulation.