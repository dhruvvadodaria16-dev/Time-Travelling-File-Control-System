# Time-Traveling File System

A sophisticated version control system that allows users to create, modify, and navigate through different versions of files using snapshots and rollback functionality.

## Architecture Overview

### Core Components

1. **TreeNode Structure**: Represents individual file versions in a tree-like hierarchy
   - Each node contains version ID, content, creation/snapshot timestamps
   - Parent-child relationships enable version branching
   - Snapshots are immutable; non-snapshots can be modified in-place

2. **FileVersionTree**: Manages the complete version history for a single file
   - Tree structure with root as version 0
   - Hash map for O(1) version lookup by ID
   - Tracks active version, total versions, and last modified time

3. **Custom Hash Maps**: 
   - `VersionHashMap`: Maps version IDs to TreeNode pointers
   - `StringHashMap<T>`: Generic string-keyed hash map with collision handling
   - Both implement dynamic resizing at 70% load factor

4. **FileHeap**: Priority queue implementation for efficient top-N queries
   - Max-heap structure for recent files (by timestamp) and biggest trees (by version count)
   - Custom swap operations maintain filename-to-index mapping
   - Prevents duplicate entries through proper contains() logic

5. **FileSystem**: Main orchestrator managing all files and operations

## Key Design Decisions

### Version Management Strategy
- **Snapshots vs Live Versions**: Only snapshotted versions are immutable
- **In-place Modification**: Non-snapshotted active versions can be modified directly
- **Branching Support**: New versions branch from current active version

### Heap Implementation
- **Dual Priority System**: Separate heaps for recent activity and version count
- **Max-heap Property**: Highest priority files appear first in queries
- **Duplicate Prevention**: Fixed StringHashMap.contains() to handle zero-indexed entries

### Memory Management
- **RAII Principles**: Destructors handle cleanup of tree structures
- **Efficient Lookups**: O(1) average case for file and version lookups
- **Dynamic Resizing**: Hash tables grow automatically to maintain performance

## Compilation

### Using the provided script:
```bash
chmod +x compile.sh
./compile.sh
```

### Manual compilation:
```bash
g++ -std=c++17 -O2 -Wall -o filesystem sol.cpp
```

## Running the Program

```bash
./filesystem
```

The program reads commands from stdin. Enter one command per line.

## Command Reference

| Command | Syntax | Description |
|---------|--------|-------------|
| `CREATE` | `CREATE <filename>` | Create a new file with initial empty snapshot (version 0) |
| `INSERT` | `INSERT <filename> <content>` | Append content to the active version |
| `UPDATE` | `UPDATE <filename> <content>` | Replace content of the active version |
| `SNAPSHOT` | `SNAPSHOT <filename> <message>` | Create an immutable snapshot of active version |
| `ROLLBACK` | `ROLLBACK <filename> [versionID]` | Rollback to specific version or parent |
| `READ` | `READ <filename>` | Display current active version content |
| `HISTORY` | `HISTORY <filename>` | Show all snapshots in the current branch |
| `RECENT_FILES` | `RECENT_FILES [N]` | Show N most recently modified files (default: 3) |
| `BIGGEST_TREES` | `BIGGEST_TREES [N]` | Show N files with most versions (default: 3) |

## Example Usage

```bash
# Create and modify files
CREATE main.cpp
INSERT main.cpp "#include <iostream>"
UPDATE main.cpp "#include <iostream>\nint main() { return 0; }"
SNAPSHOT main.cpp "Initial working version"

# Create another file
CREATE data.txt
INSERT data.txt "Sample data"
SNAPSHOT data.txt "First data entry"
INSERT data.txt "\nMore data"

# View file status
READ main.cpp
HISTORY main.cpp
RECENT_FILES 5
BIGGEST_TREES 3

# Navigate versions
ROLLBACK main.cpp 0
READ main.cpp
ROLLBACK main.cpp 2
```

## Implementation Details

### Snapshot Behavior
- **New Version Creation**: If active version is snapshotted, INSERT/UPDATE creates a new child version
- **In-place Modification**: If active version is not snapshotted, content is modified directly
- **Immutability**: Once snapshotted, a version's content cannot be changed

### Priority Systems
- **Recent Priority**: Timestamp-based counter incremented on each file operation
- **Biggest Priority**: Based on total number of versions in the file tree
- **Heap Updates**: Both heaps updated simultaneously when files are modified

### Error Handling
- Validates file existence before operations
- Handles rollback to non-existent versions
- Prevents rollback from root version without parent
- Input validation for command syntax

### Performance Characteristics
- **File Operations**: O(1) average case lookup
- **Version Access**: O(1) version retrieval by ID
- **Top-N Queries**: O(N log k) where k is total files
- **Memory**: O(V) where V is total versions across all files

## Technical Notes

### Hash Map Collision Resolution
Uses chaining with linked lists. The critical fix in `StringHashMap::contains()` properly handles cases where valid indices could be 0, preventing duplicate heap entries.

### Heap Maintenance
The max-heap implementation ensures that `RECENT_FILES` and `BIGGEST_TREES` return results in descending order of priority. The `top_n()` method uses STL heap operations for reliability.

### Cross-Platform Compatibility
Includes platform-specific time handling for both MSVC and POSIX systems, ensuring proper timestamp formatting across different environments.

## Development Notes

This implementation overcame several key challenges:
1. **Heap Ordering**: Fixed min/max heap confusion for proper priority-based sorting
2. **Duplicate Prevention**: Corrected hash map contains() logic for zero-index handling  
3. **Memory Management**: Implemented proper cleanup for tree structures
4. **Priority Synchronization**: Ensured consistent priority updates across dual heap system

The result is a robust, efficient file versioning system capable of handling complex branching scenarios while maintaining fast query performance.
