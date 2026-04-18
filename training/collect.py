"""
EdgeGaze — Data Collection Script
Run: python collect.py

Controls:
  SPACE  → capture current frame
  Q      → quit / move to next class
  R      → reset count for current class
"""

import cv2
import os
import sys
import time

CLASSES = ["happy", "angry", "sad", "neutral"]
IMG_SIZE = 48
SAMPLES_PER_CLASS = 300
DATA_DIR = "data"

def collect():
    os.makedirs(DATA_DIR, exist_ok=True)
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("[ERROR] Cannot open webcam.")
        sys.exit(1)

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    face_cascade = cv2.CascadeClassifier(
        cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
    )

    for label in CLASSES:
        save_dir = os.path.join(DATA_DIR, label)
        os.makedirs(save_dir, exist_ok=True)
        count = len(os.listdir(save_dir))

        print(f"\n{'='*50}")
        print(f"  Class: {label.upper()}  ({count}/{SAMPLES_PER_CLASS} already saved)")
        print(f"  Make a '{label}' face and press SPACE to capture.")
        print(f"  Press Q when done with this class.")
        print(f"{'='*50}\n")

        auto_mode = False

        while count < SAMPLES_PER_CLASS:
            ret, frame = cap.read()
            if not ret:
                continue

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            faces = face_cascade.detectMultiScale(gray, 1.3, 5, minSize=(60, 60))

            display = frame.copy()

            for (x, y, w, h) in faces:
                cv2.rectangle(display, (x, y), (x+w, y+h), (0, 255, 180), 2)
                face_roi = gray[y:y+h, x:x+w]
                face_roi = cv2.resize(face_roi, (IMG_SIZE, IMG_SIZE))

                if auto_mode:
                    time.sleep(0.08)
                    fname = os.path.join(save_dir, f"{count:04d}.jpg")
                    cv2.imwrite(fname, face_roi)
                    count += 1

            # HUD overlay
            cv2.putText(display, f"Emotion: {label.upper()}", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 180), 2)
            cv2.putText(display, f"Captured: {count}/{SAMPLES_PER_CLASS}", (10, 65),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 1)
            cv2.putText(display, f"Auto: {'ON' if auto_mode else 'OFF'} (A to toggle)", (10, 95),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (200, 200, 200), 1)
            cv2.putText(display, "SPACE=capture  Q=next class  R=reset  A=auto", (10, 460),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (180, 180, 180), 1)

            if len(faces) == 0:
                cv2.putText(display, "No face detected", (10, 130),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 100, 255), 2)

            cv2.imshow("EdgeGaze - Data Collection", display)
            key = cv2.waitKey(1) & 0xFF

            if key == ord('q'):
                break
            elif key == ord('r'):
                count = 0
                for f in os.listdir(save_dir):
                    os.remove(os.path.join(save_dir, f))
                print(f"  [RESET] {label} cleared.")
            elif key == ord('a'):
                auto_mode = not auto_mode
                print(f"  Auto mode: {'ON' if auto_mode else 'OFF'}")
            elif key == ord(' ') and len(faces) > 0:
                x, y, w, h = faces[0]
                face_roi = gray[y:y+h, x:x+w]
                face_roi = cv2.resize(face_roi, (IMG_SIZE, IMG_SIZE))
                fname = os.path.join(save_dir, f"{count:04d}.jpg")
                cv2.imwrite(fname, face_roi)
                count += 1
                print(f"  Saved {fname}")

        print(f"  Done with '{label}' — {count} samples.")

    cap.release()
    cv2.destroyAllWindows()
    print("\n[DONE] All classes collected. Run train.py next.")

if __name__ == "__main__":
    collect()