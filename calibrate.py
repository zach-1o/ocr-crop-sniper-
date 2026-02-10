import cv2
import json

# ===== CONFIG =====
DISPLAY_SCALE = 0.6   # try 0.5 / 0.6 / 0.7 depending on screen
# ==================

img = cv2.imread("screenshot.png")
if img is None:
    raise RuntimeError("screenshot.png not found")

H, W = img.shape[:2]

disp = cv2.resize(
    img,
    (int(W * DISPLAY_SCALE), int(H * DISPLAY_SCALE))
)

state = {
    "step": 0,      # 0:x1, 1:x2, 2+: y points
    "x1": None,
    "x2": None,
    "ys": []
}

def mouse_cb(event, x, y, flags, param):
    if event != cv2.EVENT_LBUTTONDOWN:
        return

    # Map display coords -> original coords
    ox = int(x / DISPLAY_SCALE)
    oy = int(y / DISPLAY_SCALE)

    if state["step"] == 0:
        state["x1"] = ox
        state["step"] = 1
        print("x1 =", ox)

    elif state["step"] == 1:
        state["x2"] = ox
        state["step"] = 2
        print("x2 =", ox)

    else:
        state["ys"].append(oy)
        print(f"y{len(state['ys'])} =", oy)

cv2.namedWindow("calibrate", cv2.WINDOW_NORMAL)
cv2.resizeWindow("calibrate", disp.shape[1], disp.shape[0])
cv2.setMouseCallback("calibrate", mouse_cb)

while True:
    vis = img.copy()

    if state["x1"] is not None:
        cv2.line(vis, (state["x1"], 0), (state["x1"], H), (0,255,0), 2)
    if state["x2"] is not None:
        cv2.line(vis, (state["x2"], 0), (state["x2"], H), (0,255,0), 2)

    for y in state["ys"]:
        cv2.line(vis, (0, y), (W, y), (255,0,0), 2)

    # Resize for display
    vis_disp = cv2.resize(
        vis,
        (int(W * DISPLAY_SCALE), int(H * DISPLAY_SCALE))
    )

    cv2.imshow("calibrate", vis_disp)
    key = cv2.waitKey(30)

    if key == 13 and len(state["ys"]) == 14:  # ENTER
        break
    if key == 27:  # ESC
        cv2.destroyAllWindows()
        exit()

cv2.destroyAllWindows()

x1, x2 = sorted([state["x1"], state["x2"]])

offers = []
for i in range(0, 14, 2):
    y1, y2 = sorted([state["ys"][i], state["ys"][i+1]])
    offers.append({"y1": y1, "y2": y2})

calibration = {
    "x1": x1,
    "x2": x2,
    "offers": offers
}

with open("calibration.json", "w") as f:
    json.dump(calibration, f, indent=2)

print("\nSaved calibration.json")
