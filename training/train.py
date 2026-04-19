"""
train.py  — EdgeWatch v3 (bulletproof)
---------------------------------------
Uses Keras ImageDataGenerator instead of tf.data + augmentation layers.
This avoids the gradient corruption issue that caused loss to stick at 1.38.

Data layout expected:
    data/
      train/
        happy/  neutral/  angry/  sad/  fear/  disgust/  surprise/
      test/
        (same)

Usage:
    python train.py
    python train.py --data_dir ../data --epochs 60
"""

import os
import sys
import argparse
import shutil
from collections import Counter
import numpy as np

os.environ["TF_CPP_MIN_LOG_LEVEL"] = "2"
os.environ["TF_ENABLE_ONEDNN_OPTS"] = "0"

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
from tensorflow.keras.preprocessing.image import ImageDataGenerator
from sklearn.metrics import classification_report, confusion_matrix
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import seaborn as sns

# ─────────────────────────────────────────────
# Config
# ─────────────────────────────────────────────
IMG_SIZE    = 48
BATCH       = 32
EPOCHS      = 60
SEED        = 42
NUM_CLASSES = 4

# Map FER-2013's 7 folders → 4 merged class folders
REMAP = {
    "happy":    "happy",
    "neutral":  "neutral",
    "surprise": "neutral",
    "angry":    "stressed",
    "disgust":  "stressed",
    "fear":     "stressed",
    "sad":      "sad",
}

# ─────────────────────────────────────────────
# Build merged directory
# ─────────────────────────────────────────────

def build_merged_dir(data_root: str, merged_root: str, split: str) -> str:
    src = os.path.join(data_root, split)
    dst = os.path.join(merged_root, split)

    if os.path.isdir(dst):
        # Already built — just print counts
        for cls in sorted(os.listdir(dst)):
            cls_path = os.path.join(dst, cls)
            if os.path.isdir(cls_path):
                n = len([f for f in os.listdir(cls_path)
                         if f.lower().endswith((".png",".jpg",".jpeg"))])
                print(f"    {cls:<12} {n:>5} (cached)")
        return dst

    print(f"  Building {split} → {dst}")
    for orig, merged in REMAP.items():
        src_cls = os.path.join(src, orig)
        dst_cls = os.path.join(dst, merged)
        if not os.path.isdir(src_cls):
            print(f"    WARN: {src_cls} not found")
            continue
        os.makedirs(dst_cls, exist_ok=True)
        for fname in os.listdir(src_cls):
            if not fname.lower().endswith((".png",".jpg",".jpeg")):
                continue
            src_f = os.path.join(src_cls, fname)
            dst_f = os.path.join(dst_cls, f"{orig}_{fname}")
            if not os.path.exists(dst_f):
                shutil.copy2(src_f, dst_f)

    for cls in sorted(os.listdir(dst)):
        cls_path = os.path.join(dst, cls)
        if os.path.isdir(cls_path):
            n = len([f for f in os.listdir(cls_path)
                     if f.lower().endswith((".png",".jpg",".jpeg"))])
            print(f"    {cls:<12} {n:>5}")
    return dst

# ─────────────────────────────────────────────
# Model
# ─────────────────────────────────────────────

def build_model():
    inp = keras.Input(shape=(IMG_SIZE, IMG_SIZE, 1))

    # Block 1 — 48→24
    x = layers.Conv2D(32, 3, padding="same")(inp)
    x = layers.BatchNormalization()(x)
    x = layers.Activation("relu")(x)
    x = layers.Conv2D(32, 3, padding="same")(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation("relu")(x)
    x = layers.MaxPooling2D(2)(x)
    x = layers.Dropout(0.3)(x)

    # Block 2 — 24→12
    x = layers.Conv2D(64, 3, padding="same")(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation("relu")(x)
    x = layers.Conv2D(64, 3, padding="same")(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation("relu")(x)
    x = layers.MaxPooling2D(2)(x)
    x = layers.Dropout(0.3)(x)

    # Block 3 — 12→6
    x = layers.Conv2D(128, 3, padding="same")(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation("relu")(x)
    x = layers.MaxPooling2D(2)(x)
    x = layers.Dropout(0.3)(x)

    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dense(256, activation="relu")(x)
    x = layers.Dropout(0.5)(x)
    out = layers.Dense(NUM_CLASSES, activation="softmax", name="output")(x)

    return keras.Model(inp, out, name="EdgeWatch_CNN")

# ─────────────────────────────────────────────
# Train
# ─────────────────────────────────────────────

def train(args):
    tf.random.set_seed(SEED)
    np.random.seed(SEED)

    # ── 1. Build merged dirs ──
    merged_root = os.path.join(args.data_dir, "_merged")
    print("\n[1/4] Merging class folders...")
    train_dir = build_merged_dir(args.data_dir, merged_root, "train")
    val_dir   = build_merged_dir(args.data_dir, merged_root, "test")

    # ── 2. Data generators ──
    print("\n[2/4] Creating generators...")
    train_gen = ImageDataGenerator(
        rescale=1.0/255,
        rotation_range=15,
        width_shift_range=0.1,
        height_shift_range=0.1,
        zoom_range=0.1,
        horizontal_flip=True,
        brightness_range=[0.8, 1.2],
        fill_mode="nearest",
    ).flow_from_directory(
        train_dir,
        target_size=(IMG_SIZE, IMG_SIZE),
        color_mode="grayscale",
        batch_size=BATCH,
        class_mode="sparse",
        shuffle=True,
        seed=SEED,
    )

    val_gen = ImageDataGenerator(
        rescale=1.0/255,
    ).flow_from_directory(
        val_dir,
        target_size=(IMG_SIZE, IMG_SIZE),
        color_mode="grayscale",
        batch_size=BATCH,
        class_mode="sparse",
        shuffle=False,
    )

    print(f"\n  Class indices: {train_gen.class_indices}")
    print(f"  Train: {train_gen.samples}  Val: {val_gen.samples}")

    # Sanity check a batch
    x_b, y_b = next(train_gen)
    print(f"\n  Batch sanity: shape={x_b.shape}  "
          f"range=[{x_b.min():.3f}, {x_b.max():.3f}]  "
          f"mean={x_b.mean():.3f}")
    if x_b.mean() < 0.01:
        print("CRITICAL: Batch is all zeros — data path is wrong. Aborting.")
        sys.exit(1)

    # Class weights
    counts = Counter(train_gen.classes)
    total  = sum(counts.values())
    class_weights = {cls: total / (NUM_CLASSES * cnt)
                     for cls, cnt in counts.items()}
    idx_to_name = {v: k for k, v in train_gen.class_indices.items()}
    print(f"\n  Class weights: { {idx_to_name[i]: f'{w:.2f}' for i,w in class_weights.items()} }")

    # ── 3. Model ──
    print("\n[3/4] Building model...")
    model = build_model()
    model.summary()
    print(f"  Params: {model.count_params():,}")

    model.compile(
        optimizer=keras.optimizers.Adam(1e-3),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )

    cbs = [
        keras.callbacks.ReduceLROnPlateau(
            monitor="val_accuracy", factor=0.5,
            patience=5, min_lr=1e-6, verbose=1,
        ),
        keras.callbacks.EarlyStopping(
            monitor="val_accuracy", patience=15,
            restore_best_weights=True, verbose=1,
        ),
        keras.callbacks.ModelCheckpoint(
            "emotion_model_best.keras",
            monitor="val_accuracy",
            save_best_only=True, verbose=1,
        ),
    ]

    # ── 4. Train ──
    print(f"\n[4/4] Training (up to {args.epochs} epochs)...")
    history = model.fit(
        train_gen,
        validation_data=val_gen,
        epochs=args.epochs,
        class_weight=class_weights,
        callbacks=cbs,
    )

    # ── Evaluate ──
    print("\n── Final evaluation ──")
    val_gen.reset()
    probs  = model.predict(val_gen, verbose=1)
    y_pred = np.argmax(probs, axis=1)
    y_true = val_gen.classes[:len(y_pred)]

    label_order = [idx_to_name[i] for i in range(NUM_CLASSES)]
    acc = (y_pred == y_true).mean()
    print(f"\n  Val accuracy: {acc*100:.2f}%\n")
    print(classification_report(y_true, y_pred, target_names=label_order))

    model.save("emotion_model.keras")
    print("Saved: emotion_model.keras  |  emotion_model_best.keras")

    _save_plots(history, y_true, y_pred, label_order)


def _save_plots(history, y_true, y_pred, label_names):
    fig, axes = plt.subplots(1, 2, figsize=(13, 4))
    axes[0].plot(history.history["accuracy"],     label="Train")
    axes[0].plot(history.history["val_accuracy"], label="Val")
    axes[0].set_title("Accuracy"); axes[0].set_xlabel("Epoch"); axes[0].legend()
    axes[1].plot(history.history["loss"],     label="Train")
    axes[1].plot(history.history["val_loss"], label="Val")
    axes[1].set_title("Loss"); axes[1].set_xlabel("Epoch"); axes[1].legend()
    plt.tight_layout()
    plt.savefig("training_curves.png", dpi=120)
    print("Saved: training_curves.png")
    plt.close()

    cm = confusion_matrix(y_true, y_pred)
    cm_norm = cm.astype(float) / cm.sum(axis=1, keepdims=True)
    fig, ax = plt.subplots(figsize=(6, 5))
    sns.heatmap(cm_norm, annot=True, fmt=".2f",
                xticklabels=label_names, yticklabels=label_names,
                cmap="Blues", ax=ax, linewidths=0.5)
    ax.set_xlabel("Predicted"); ax.set_ylabel("True")
    ax.set_title("Confusion Matrix (normalised)")
    plt.tight_layout()
    plt.savefig("confusion_matrix.png", dpi=120)
    print("Saved: confusion_matrix.png")
    plt.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data_dir", default="../data")
    parser.add_argument("--epochs",   type=int, default=EPOCHS)
    args = parser.parse_args()
    train(args)
