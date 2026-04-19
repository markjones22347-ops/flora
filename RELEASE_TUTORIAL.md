# Flora Release Tutorial - Beginner Friendly

This guide will walk you through the complete process of releasing Flora, updating GitHub, and distributing the updater.

---

## 📋 Prerequisites

Before you start, make sure you have:
- GitHub account
- Git installed on your computer
- Flora project built and ready in `imgui_ui\build\Release\`
- Flora Updater built in `Flora Updater\build\bin\Release`

---

## 🚀 Step 1: Prepare Release Files

Your release directory should contain exactly these 4 files:
```
C:\Users\safes\Desktop\lua executor\Flora Executor\imgui_ui\build\Release\
├── Flora.exe
├── FloraAPI.dll
├── imgui.ini
└── version.txt
```

**Verify version.txt contains the correct version** (e.g., `1.0.0`)

---

## 📦 Step 2: Create ZIP Archive

### Option A: Using Windows (Right-click method)
1. Navigate to `C:\Users\safes\Desktop\lua executor\Flora Executor\imgui_ui\build\Release\`
2. Select all 4 files (Flora.exe, FloraAPI.dll, imgui.ini, version.txt)
3. Right-click → Send to → Compressed (zipped) folder
4. Rename the ZIP to `flora.zip`

### Option B: Using PowerShell
1. Open PowerShell in the Release directory:
   ```powershell
   cd "C:\Users\safes\Desktop\lua executor\Flora Executor\imgui_ui\build\Release"
   ```
2. Run:
   ```powershell
   Compress-Archive -Path "Flora.exe","FloraAPI.dll","imgui.ini","version.txt" -DestinationPath "flora.zip"
   ```

**Important:** The ZIP must contain the files directly, NOT in a subfolder!

---

## 📝 Step 3: Create update.json

Create a file named `update.json` with the following content:

```json
{
  "version": "1.0.0",
  "url": "https://github.com/YOUR_USERNAME/Flora/releases/download/v1.0.0/flora.zip"
}
```

**Replace:**
- `YOUR_USERNAME` with your actual GitHub username
- `1.0.0` with your actual version number (both places)

Save this file in your Flora GitHub repository root.

---

## 🌐 Step 4: Push update.json to GitHub

### Option A: Using GitHub Website
1. Go to your Flora repository on GitHub
2. Click "Add file" → "Create new file"
3. Name it `update.json`
4. Paste the JSON content from Step 3
5. Click "Commit changes"

### Option B: Using Git Command Line
1. Navigate to your Flora repository folder
2. Create/update update.json:
   ```powershell
   cd "C:\Users\safes\Desktop\lua executor\Flora Executor"
   echo '{"version":"1.0.0","url":"https://github.com/YOUR_USERNAME/Flora/releases/download/v1.0.0/flora.zip"}' > update.json
   ```
3. Commit and push:
   ```powershell
   git add update.json
   git commit -m "Update update.json to version 1.0.0"
   git push
   ```

---

## 🎯 Step 5: Create GitHub Release

### Method A: Using GitHub Website (Recommended)
1. Go to your Flora repository on GitHub
2. Click "Releases" on the right sidebar
3. Click "Create a new release"
4. Tag version: `v1.0.0` (must start with 'v')
5. Release title: `Version 1.0.0`
6. Description (optional):
   ```text
   ## Flora v1.0.0
   
   ### Features
   - Multi-tab script editor
   - Tab persistence
   - Auto-updater
   - And more...
   ```
7. Click "Choose files" → Select `flora.zip` from Step 2
8. Click "Publish release"

### Method B: Using GitHub CLI (gh)
1. Install GitHub CLI if not already installed
2. Run:
   ```powershell
   gh release create v1.0.0 "C:\Users\safes\Desktop\lua executor\Flora Executor\imgui_ui\build\Release\flora.zip" --title "Version 1.0.0" --notes "Flora v1.0.0 Release"
   ```

---

## 🔄 Step 6: Update Flora Updater Configuration

Before distributing the updater, update the GitHub URL:

1. Open `Flora Updater\src\updater.cpp`
2. Find line 12:
   ```cpp
   const std::string UPDATE_JSON_URL = "https://raw.githubusercontent.com/YOUR_USER/Flora/main/update.json";
   ```
3. Replace `YOUR_USER` with your actual GitHub username
4. Rebuild the updater:
   ```powershell
   cd "C:\Users\safes\Desktop\lua executor\Flora Executor\Flora Updater"
   cmake --build build --config Release
   ```

---

## 📤 Step 7: Distribute the Updater

The updater executable is located at:
```
C:\Users\safes\Desktop\lua executor\Flora Executor\Flora Updater\build\bin\Release\FloraUpdater.exe
```

### Distribution Options:

#### Option 1: Direct Download
- Upload `FloraUpdater.exe` to your GitHub Releases
- Share the download link with users

#### Option 2: Create Installer (Optional)
- Use tools like NSIS or Inno Setup to create an installer
- The installer can:
  - Place FloraUpdater.exe in a user-accessible location
  - Create desktop shortcuts
  - Add to Start menu

#### Option 3: Web Download Page
- Create a simple HTML page with download button
- Host on GitHub Pages or your website

---

## 🧪 Step 8: Test the Complete Flow

### Test Fresh Install:
1. Delete `%APPDATA%\Flora\` folder (if exists)
2. Run `FloraUpdater.exe`
3. Should:
   - Download latest version
   - Extract to `%APPDATA%\Flora\`
   - Launch Flora.exe
   - Close after 3 seconds

### Test Update:
1. Modify `version.txt` in `%APPDATA%\Flora\` to `0.0.0`
2. Run `FloraUpdater.exe`
3. Should detect update and install new version

---

## 📋 Quick Reference Checklist

### For Each Release:
- [ ] Build Flora in Release mode
- [ ] Verify files in Release directory (4 files)
- [ ] Create flora.zip (flat structure)
- [ ] Update update.json with new version and URL
- [ ] Push update.json to GitHub
- [ ] Create GitHub Release with flora.zip
- [ ] Test updater (optional but recommended)

### First Time Setup:
- [ ] Create GitHub repository
- [ ] Update Flora Updater with your GitHub username
- [ ] Rebuild Flora Updater
- [ ] Distribute FloraUpdater.exe to users

---

## 🔧 Troubleshooting

### updater.exe not downloading update.json
- Check that update.json is in your GitHub repository root
- Verify the URL in updater.cpp matches your repository
- Ensure the repository is public

### ZIP extraction fails
- Verify ZIP contains files directly (not in subfolder)
- Check that all 4 files are present in ZIP

### Version not updating
- Ensure version.txt is in the ZIP
- Verify update.json version matches release version
- Check that version.txt contains only the version number (one line)

---

## 📚 Additional Resources

- GitHub Releases Documentation: https://docs.github.com/en/releases
- GitHub CLI Documentation: https://cli.github.com/manual/
- WinHTTP API: https://docs.microsoft.com/en-us/windows/win32/api/winhttp/

---

## ✅ Summary

**For Users:**
1. Download `FloraUpdater.exe`
2. Run it once
3. Flora installs automatically to `%APPDATA%\Flora\`
4. Future updates happen automatically when running the updater

**For Developers:**
1. Build Flora in Release mode
2. Create ZIP with 4 files
3. Update update.json
4. Create GitHub Release
5. Users get updates automatically

That's it! You now have a complete auto-updating system for Flora.
