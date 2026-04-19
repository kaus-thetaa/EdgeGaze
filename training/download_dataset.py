"""
download_dataset.py
-------------------
Downloads FER-2013 from Kaggle and verifies the folder structure.

Prerequisites:
    1. Create a Kaggle account at kaggle.com
    2. Go to Account → API → Create New Token  (downloads kaggle.json)
    3. Place kaggle.json at:
         Windows: C:\\Users\\<you>\\.kaggle\\kaggle.json
         Mac/Linux: ~/.kaggle/kaggle.json
    4. pip install kaggle

Usage:
    python download_dataset.py
    python download_dataset.py --dest ../data
"""

import os
import sys
import argparse
import zipfile


def check_kaggle_credentials():
    home = os.path.expanduser("~")
    cred_path = os.path.join(home, ".kaggle", "kaggle.json")
    if not os.path.exists(cred_path):
        print("ERROR: kaggle.json not found.")
        print(f"Expected location: {cred_path}")
        print("\nSteps to fix:")
        print("  1. Go to https://www.kaggle.com/settings → API → Create New Token")
        print("  2. Save the downloaded kaggle.json to:", cred_path)
        print("  3. Re-run this script")
        sys.exit(1)
    print(f"Kaggle credentials found: {cred_path}")


def download(dest: str):
    check_kaggle_credentials()

    try:
        import kaggle
    except ImportError:
        print("kaggle package not installed. Run: pip install kaggle")
        sys.exit(1)

    os.makedirs(dest, exist_ok=True)
    print(f"\nDownloading FER-2013 to {dest} ...")
    print("(~60MB — may take a minute)")

    kaggle.api.authenticate()
    kaggle.api.dataset_download_files(
        "msambare/fer2013",
        path=dest,
        unzip=True,
        quiet=False,
    )
    print("\nDownload complete.")
    verify(dest)


def verify(dest: str):
    expected_classes = ["happy", "neutral", "angry", "sad", "fear", "disgust", "surprise"]
    splits = ["train", "test"]
    all_ok = True

    print("\nVerifying folder structure...")
    for split in splits:
        split_path = os.path.join(dest, split)
        if not os.path.isdir(split_path):
            print(f"  MISSING: {split_path}")
            all_ok = False
            continue
        for cls in expected_classes:
            cls_path = os.path.join(split_path, cls)
            if not os.path.isdir(cls_path):
                print(f"  MISSING folder: {cls_path}")
                all_ok = False
            else:
                n = len([f for f in os.listdir(cls_path)
                         if f.lower().endswith((".png", ".jpg"))])
                status = "OK" if n > 50 else "WARN (low count)"
                print(f"  {split}/{cls:<12} {n:>5} images  [{status}]")

    if all_ok:
        print("\nAll folders present. Ready to train.")
        print(f"Run: python train.py --data_dir {dest}")
    else:
        print("\nSome folders missing. Check the download or extract manually.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--dest", default="../data",
                        help="Where to save the dataset")
    parser.add_argument("--verify-only", action="store_true",
                        help="Only check an existing download, don't re-download")
    args = parser.parse_args()

    if args.verify_only:
        verify(args.dest)
    else:
        download(args.dest)
