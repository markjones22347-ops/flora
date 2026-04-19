# Flora Executor - Tab System Changelog

## Version 1.0 - Tab System Implementation

### Core Features
- Multi-tab script editor with dynamic tab management
- Per-tab content storage (65536 byte buffer per tab)
- Tab names displayed in top bar with close (X) buttons
- Drag and drop tab reordering support
- Tab bar automatically scrolls when many tabs are open

### Data Management
- Each tab independently stores:
  - Script content (65536 bytes)
  - Cursor position
  - Selection start/end positions
  - Scroll position
  - Modified/unsaved state
- Modified indicator shows "*" next to unsaved tabs
- Dynamic tab numbering - tabs automatically renumber based on actual count
  (e.g., if you have Script 1 and Script 5, they become Script 1 and Script 2)

### Persistence
- Tabs automatically saved to AppData/Flora/tabs.dat
- Tabs restored on startup from saved data
- Auto-save on: tab creation, tab closure, content edits
- Flora data directory created automatically in AppData
- Fallback to temp directory if AppData unavailable

### User Interface
- Tab bar with ImGuiTabBarFlags_Reorderable for drag-and-drop
- Tab bar with ImGuiTabBarFlags_TabListPopupButton for context menu
- Unique widget IDs per tab to prevent content sharing
- PushID on tab items for proper ID scoping
- Editor uses unique ID per tab (##ScriptEditor_{index})

### Hotkeys
- `Ctrl+T`: Create new tab
- `Ctrl+W`: Close current tab
- `Ctrl+S`: Save tabs manually

### Technical Implementation
- ScriptTab struct stores all tab data
- Global tabs vector manages all tabs
- Active tab tracking with g_ActiveTab
- RenumberTabs() function for dynamic numbering
- SaveTabsToFile() / LoadTabsFromFile() for persistence
- GetFloraDataDirectory() for proper data storage location
