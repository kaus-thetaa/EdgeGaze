"""
diagnose.py
-----------
Run this BEFORE train.py to verify your data is loading correctly.
It will tell you exactly what's wrong.

Usage:
    python diagnose.py
    python diagnose.py --data_dir ../data
"""

import os
import sys
import argparse
import numpy as np

def diagnose(data_dir):
    print("=" * 60)
    print("EdgeWatch — Data Diagnostics")
    print("=" * 60)

    # ── 1. Check folder exists ──
    train_dir = os.path.join(data_dir, "train")
    if not os.path.isdir(train_dir):
        print(f"\nFAIL: train/ folder not found at: {os.path.abspath(train_dir)}")
        print("Your data_dir should contain train/ and test/ subfolders.")
        print(f"Current working directory: {os.getcwd()}")
        sys.exit(1)

    print(f"\nOK: Found train/ at {os.path.abspath(train_dir)}")

    # ── 2. List all subfolders ──
    folders = [f for f in os.listdir(train_dir)
               if os.path.isdir(os.path.join(train_dir, f))]
    print(f"\nFound {len(folders)} class folders: {sorted(folders)}")

    expected = {"happy", "neutral", "angry", "sad", "fear", "disgust", "surprise"}
    missing  = expected - {f.lower() for f in folders}
    if missing:
        print(f"WARN: Missing expected folders: {missing}")
    else:
        print("OK: All 7 FER-2013 class folders present.")

    # ── 3. Sample and check pixel values ──
    print("\n── Pixel value check (first 3 images per class) ──")
    all_means = []

    try:
        from PIL import Image
    except ImportError:
        print("Installing Pillow...")
        os.system(f"{sys.executable} -m pip install Pillow -q")
        from PIL import Image

    for folder in sorted(folders):
        folder_path = os.path.join(train_dir, folder)
        files = [f for f in os.listdir(folder_path)
                 if f.lower().endswith((".png", ".jpg", ".jpeg"))]

        if not files:
            print(f"  {folder:<12} — NO IMAGE FILES FOUND")
            continue

        means = []
        for fname in files[:3]:
            try:
                img = Image.open(os.path.join(folder_path, fname)).convert("L")
                img = img.resize((48, 48))
                arr = np.array(img, dtype=np.float32)
                means.append(arr.mean())
            except Exception as e:
                print(f"  {folder}/{fname} — LOAD ERROR: {e}")

        if means:
            avg = np.mean(means)
            all_means.append(avg)
            status = "OK" if 20 < avg < 235 else "WARN — possible corrupt/blank images"
            print(f"  {folder:<12}  {len(files):>5} files  avg pixel={avg:.1f}  [{status}]")

    if all_means:
        overall = np.mean(all_means)
        print(f"\n  Overall mean pixel value: {overall:.1f}  (expect 80–160 for face images)")
        if overall < 10:
            print("  FAIL: Images appear to be all black. Check your download.")
        elif overall > 245:
            print("  FAIL: Images appear to be all white.")
        else:
            print("  OK: Pixel values look normal.")

    # ── 4. Check image sizes ──
    print("\n── Image size check ──")
    sample_folder = os.path.join(train_dir, "happy")
    if os.path.isdir(sample_folder):
        sample_files = [f for f in os.listdir(sample_folder)
                        if f.lower().endswith((".png", ".jpg", ".jpeg"))][:5]
        for fname in sample_files:
            try:
                img = Image.open(os.path.join(sample_folder, fname))
                print(f"  {fname}: {img.size} mode={img.mode}")
            except Exception as e:
                print(f"  {fname}: ERROR {e}")

    # ── 5. Quick model sanity test ──
    print("\n── Quick model / TF check ──")
    try:
        import tensorflow as tf
        print(f"  TensorFlow version: {tf.__version__}")

        # Build tiny model and do one forward pass
        model = tf.keras.Sequential([
            tf.keras.layers.Input(shape=(48, 48, 1)),
            tf.keras.layers.Conv2D(8, 3, padding="same", activation="relu"),
            tf.keras.layers.GlobalAveragePooling2D(),
            tf.keras.layers.Dense(4, activation="softmax"),
        ])
        dummy = np.random.rand(2, 48, 48, 1).astype(np.float32)
        out = model(dummy, training=False)
        print(f"  Dummy forward pass OK: output shape {out.shape}")
        print(f"  Output values: {out.numpy().round(3)}")

        # Check if outputs are uniform (bad) or varied (good)
        if np.std(out.numpy()) < 0.01:
            print("  WARN: All outputs nearly identical — may indicate init issue")
        else:
            print("  OK: Output values varied (model is initialising correctly)")

    except Exception as e:
        print(f"  TF check failed: {e}")

    # ── 6. Summary ──
    print("\n" + "=" * 60)
    print("If everything above shows OK, run:")
    print("    python train.py --data_dir", data_dir)
    print("=" * 60)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data_dir", default="../data")
    args = parser.parse_args()
    diagnose(args.data_dir)
