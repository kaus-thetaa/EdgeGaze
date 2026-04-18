"""
EdgeGaze — Model Training Script
Run: python train.py

Trains a small CNN on the collected face emotion dataset.
Target: <80KB after INT8 quantization, >88% val accuracy.
"""

import os
import numpy as np
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import seaborn as sns

os.environ["TF_CPP_MIN_LOG_LEVEL"] = "2"
import tensorflow as tf
from tensorflow.keras import layers, models, callbacks
from tensorflow.keras.preprocessing.image import ImageDataGenerator

# ── Config ───────────────────────────────────────────────────────────────────
CLASSES      = ["happy", "angry", "sad", "neutral"]
IMG_SIZE     = 48
DATA_DIR     = "data"
MODEL_PATH   = "emotion_model.h5"
BATCH_SIZE   = 32
EPOCHS       = 50
SEED         = 42


# ── Data loading ──────────────────────────────────────────────────────────────
def load_dataset():
    X, y = [], []
    for idx, label in enumerate(CLASSES):
        folder = os.path.join(DATA_DIR, label)
        if not os.path.exists(folder):
            print(f"[WARN] Missing folder: {folder}")
            continue
        files = [f for f in os.listdir(folder) if f.endswith(".jpg")]
        print(f"  {label}: {len(files)} images")
        for fname in files:
            img = tf.keras.preprocessing.image.load_img(
                os.path.join(folder, fname),
                color_mode="grayscale",
                target_size=(IMG_SIZE, IMG_SIZE),
            )
            arr = tf.keras.preprocessing.image.img_to_array(img) / 255.0
            X.append(arr)
            y.append(idx)

    X = np.array(X, dtype="float32")
    y = np.array(y, dtype="int32")
    print(f"\n  Total: {len(X)} samples across {len(CLASSES)} classes")
    return X, y


# ── Model architecture ────────────────────────────────────────────────────────
def build_model():
    model = models.Sequential([
        # Block 1
        layers.Conv2D(16, (3, 3), padding="same", activation="relu",
                      input_shape=(IMG_SIZE, IMG_SIZE, 1)),
        layers.BatchNormalization(),
        layers.MaxPooling2D(2, 2),
        layers.Dropout(0.2),

        # Block 2
        layers.Conv2D(32, (3, 3), padding="same", activation="relu"),
        layers.BatchNormalization(),
        layers.MaxPooling2D(2, 2),
        layers.Dropout(0.2),

        # Block 3
        layers.Conv2D(64, (3, 3), padding="same", activation="relu"),
        layers.BatchNormalization(),
        layers.MaxPooling2D(2, 2),
        layers.Dropout(0.3),

        # Classifier head
        layers.Flatten(),
        layers.Dense(128, activation="relu"),
        layers.Dropout(0.4),
        layers.Dense(len(CLASSES), activation="softmax"),
    ], name="EdgeGaze_CNN")

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=1e-3),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    model.summary()
    return model


# ── Training ──────────────────────────────────────────────────────────────────
def train():
    print("\n[1/4] Loading dataset...")
    X, y = load_dataset()

    X_train, X_val, y_train, y_val = train_test_split(
        X, y, test_size=0.2, random_state=SEED, stratify=y
    )
    print(f"  Train: {len(X_train)}  Val: {len(X_val)}")

    # Augmentation (only on training data)
    datagen = ImageDataGenerator(
        rotation_range=15,
        width_shift_range=0.1,
        height_shift_range=0.1,
        zoom_range=0.1,
        horizontal_flip=True,
        brightness_range=[0.8, 1.2],
    )

    print("\n[2/4] Building model...")
    model = build_model()

    cbs = [
        callbacks.EarlyStopping(patience=8, restore_best_weights=True, verbose=1),
        callbacks.ReduceLROnPlateau(factor=0.5, patience=4, verbose=1),
        callbacks.ModelCheckpoint(MODEL_PATH, save_best_only=True, verbose=0),
    ]

    print("\n[3/4] Training...")
    history = model.fit(
        datagen.flow(X_train, y_train, batch_size=BATCH_SIZE),
        validation_data=(X_val, y_val),
        epochs=EPOCHS,
        callbacks=cbs,
        verbose=1,
    )

    print("\n[4/4] Evaluation...")
    model = tf.keras.models.load_model(MODEL_PATH)
    loss, acc = model.evaluate(X_val, y_val, verbose=0)
    print(f"\n  Val accuracy : {acc*100:.2f}%")
    print(f"  Val loss     : {loss:.4f}")

    y_pred = np.argmax(model.predict(X_val, verbose=0), axis=1)
    print("\n  Per-class report:")
    print(classification_report(y_val, y_pred, target_names=CLASSES))

    # ── Plots ─────────────────────────────────────────────────────────────────
    fig, axes = plt.subplots(1, 3, figsize=(18, 5))
    fig.suptitle("EdgeGaze Training Results", fontsize=14, fontweight="bold")

    # Accuracy
    axes[0].plot(history.history["accuracy"],    label="Train acc")
    axes[0].plot(history.history["val_accuracy"], label="Val acc")
    axes[0].set_title("Accuracy")
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)

    # Loss
    axes[1].plot(history.history["loss"],    label="Train loss")
    axes[1].plot(history.history["val_loss"], label="Val loss")
    axes[1].set_title("Loss")
    axes[1].legend()
    axes[1].grid(True, alpha=0.3)

    # Confusion matrix
    cm = confusion_matrix(y_val, y_pred)
    sns.heatmap(cm, annot=True, fmt="d", ax=axes[2],
                xticklabels=CLASSES, yticklabels=CLASSES, cmap="Blues")
    axes[2].set_title("Confusion Matrix")
    axes[2].set_xlabel("Predicted")
    axes[2].set_ylabel("True")

    plt.tight_layout()
    plt.savefig("training_results.png", dpi=150)
    print("\n  Saved training_results.png")
    plt.show()

    print(f"\n  Model saved → {MODEL_PATH}")
    print("  Run convert_tflite.py next.\n")


if __name__ == "__main__":
    train()