import os

# --- 1. Define the Directory Structure ---
directories = [
    "EdgeGaze/training",
    "EdgeGaze/firmware",
    "EdgeGaze/web_dashboard",
    "EdgeGaze/assets"
]

# --- 2. Define the File Contents ---
files = {}

# README.md (Using your latest version)
files["EdgeGaze/README.md"] = ""

# requirements.txt
files["EdgeGaze/training/requirements.txt"] = ""

# collect.py
files["EdgeGaze/training/collect.py"] = ""

# train.py
files["EdgeGaze/training/train.py"] = ""

# convert_tflite.py
files["EdgeGaze/training/convert_tflite.py"] = ""

# Web Dashboard
files["EdgeGaze/web_dashboard/index.html"] = ""

# Firmware - config.h
files["EdgeGaze/firmware/config.h"] = ""

# Firmware - EdgeGaze.ino (Monolithic main file to guarantee it compiles easily for 3 days)
files["EdgeGaze/firmware/EdgeGaze.ino"] = ""

# Firmware - Task Headers & CPP Skeletons
tasks = ["camera", "inference", "display", "web"]
for task in tasks:
    # Header
    files[f"EdgeGaze/firmware/{task}_task.h"] = ""
    # CPP
    files[f"EdgeGaze/firmware/{task}_task.cpp"] = ""

# Dummy model.h
files["EdgeGaze/firmware/model.h"] = ""

# --- 3. Build the Workspace ---
print("🚀 Building EdgeGaze Workspace...")

for directory in directories:
    os.makedirs(directory, exist_ok=True)
    print(f"📁 Created directory: {directory}")

for filepath, content in files.items():
    with open(filepath, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"📄 Created file: {filepath}")

print("\\n✅ Workspace built successfully! Open the 'EdgeGaze' folder in VS Code to get started.")