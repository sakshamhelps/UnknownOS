# GitHub Actions Build Guide - Unknown OS

**Builds your ISO in the cloud for FREE. No local Linux needed.**

---

## Step-by-Step (5 minutes setup)

### 1. Create GitHub Account
- Go to https://github.com → Sign up (free)

### 2. Create New Repository
- Click **"+"** (top right) → **"New repository"**
- Name: `unknown-os` (or whatever)
- **Public** or **Private** (both work)
- **Don't** initialize with README/License
- Click **"Create repository"**

### 3. Upload Your Files
**Option A: Web Upload (Easiest)**
1. In your new repo → Click **"uploading an existing file"**
2. Drag **entire `iso-profile` folder contents** (not the folder itself)
3. Wait for upload → Commit message: `"Initial Unknown OS build profile"` → Commit

**Option B: GitHub Desktop (If you prefer GUI)**
1. Install GitHub Desktop
2. Clone your empty repo
3. Copy all files from `C:\Users\GODX\UnknownOS\iso-profile\` into the repo folder
4. Commit → Push

**Option C: Command Line**
```bash
cd C:\Users\GODX\UnknownOS\iso-profile
git init
git add .
git commit -m "Initial Unknown OS build profile"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/unknown-os.git
git push -u origin main
```

### 4. Trigger the Build
**Automatic:** Build runs on every push to main.

**Manual (Recommended first time):**
1. Go to your repo on GitHub
2. Click **"Actions"** tab (top menu)
3. Click **"Build Unknown OS ISO"** (left sidebar)
4. Click **"Run workflow"** → **"Run workflow"** (green button)
5. Wait...

### 5. Watch the Build
- Click the running workflow
- See live logs (takes 20-45 minutes)
- Green checkmark = success

### 6. Download the ISO
**After success:**
1. Click the completed workflow run
2. Scroll down to **"Artifacts"**
3. Click **"UnknownOS-ISO"** → Downloads `UnknownOS-ISO.zip`
4. Unzip → You get `UnknownOS-1.0.0-IND-x86_64.iso`

### 7. Burn to USB
- Use **Rufus** (Windows) / **BalenaEtcher** (cross-platform)
- Select ISO → Select USB → Flash
- Boot from USB (F12) → Test → Install

---

## Pro Tips

### Build on Tag = Auto Release
```bash
# Create a version tag
git tag -a v1.0.0 -m "Unknown OS 1.0.0-IND"
git push origin v1.0.0
```
→ Creates a **GitHub Release** with ISO attached for easy download.

### Faster Subsequent Builds
- GitHub caches packages automatically
- Only changed files rebuild

### Debug Failed Builds
- Click failed workflow → Expand red step → Read logs
- Common fix: Increase timeout (currently 120 min in workflow)

---

## File Structure in Repo
```
your-repo/
├── .github/workflows/build-iso.yml  ← Already created
├── build.sh                          ← Already created
├── profiledef.sh
├── packages.x86_64
├── airootfs/                         ← All OS files
├── syslinux/
├── efiboot/
└── ... (all files from iso-profile)
```

---

## Troubleshooting

| Issue | Fix |
|-------|-----|
| Build times out | Increase `timeout-minutes: 180` in workflow |
| Out of space | GitHub gives ~70GB - should be enough |
| Permission denied | Ensure `build.sh` is executable (`chmod +x`) |
| Package not found | Check `packages.x86_64` names match Arch repos |

---

## Support
- **Email:** byrex.me@proton.me
- Check workflow logs first - they show exact error

---

**That's it. Push files → Click "Run workflow" → Download ISO.**