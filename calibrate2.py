import pyautogui
import json
import time

print("\n=== CALIBRATION MODE ===")
print("You will be asked to click on screen positions.")
print("Move mouse and LEFT CLICK when prompted.\n")
time.sleep(2)

data = {
    "x1": None,
    "x2": None,
    "offers": [],
    "buy_buttons": [],
    "tabs": {}
}

# ---- Vertical strip (x1, x2) ----
print("STEP 1: Vertical strip")
print("Click LEFT edge of offer column (x1)")
x1, _ = pyautogui.position()
input("Press ENTER then click...")
time.sleep(0.2)
x1, _ = pyautogui.position()

print("Click RIGHT edge of offer column (x2)")
input("Press ENTER then click...")
time.sleep(0.2)
x2, _ = pyautogui.position()

data["x1"] = min(x1, x2)
data["x2"] = max(x1, x2)

# ---- Offer rows ----
print("\nSTEP 2: Offer rows (₹ text)")
for i in range(7):
    print(f"Click TOP of offer row {i+1}")
    input("Press ENTER then click...")
    time.sleep(0.2)
    _, y1 = pyautogui.position()

    print(f"Click BOTTOM of offer row {i+1}")
    input("Press ENTER then click...")
    time.sleep(0.2)
    _, y2 = pyautogui.position()

    data["offers"].append({
        "y1": min(y1, y2),
        "y2": max(y1, y2)
    })

# ---- Buy buttons ----
print("\nSTEP 3: Buy buttons")
for i in range(7):
    print(f"Click CENTER of BUY button for offer {i+1}")
    input("Press ENTER then click...")
    time.sleep(0.2)
    x, y = pyautogui.position()
    data["buy_buttons"].append({ "x": x, "y": y })

# ---- Tabs ----
print("\nSTEP 4: Tabs")
print("Click DEFAULT tab")
input("Press ENTER then click...")
time.sleep(0.2)
x, y = pyautogui.position()
data["tabs"]["default"] = { "x": x, "y": y }

print("Click LARGE tab")
input("Press ENTER then click...")
time.sleep(0.2)
x, y = pyautogui.position()
data["tabs"]["large"] = { "x": x, "y": y }

# ---- Save ----
with open("calibration.json", "w") as f:
    json.dump(data, f, indent=2)

print("\n✅ Calibration complete.")
print("Saved as calibration.json")
