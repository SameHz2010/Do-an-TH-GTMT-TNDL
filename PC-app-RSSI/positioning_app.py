import asyncio
import struct
import os
import time
import matplotlib
matplotlib.use('Agg') 
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from bleak import BleakScanner
import numpy as np

# ==========================================
# 1. CẤU HÌNH KHÔNG GIAN (SCALE THỰC TẾ)
# ==========================================

# Tỷ lệ: 1 đơn vị = 0.2m thực tế (2 gang tay ~ 0.4m = 2 đơn vị)
REAL_SCALE = 0.2  # mét/đơn vị

ROOM_MAP = {
    "Phòng 1": [0, 4, 0, 4],
    "Phòng 2": [4, 8, 0, 4],
    "Phòng 3": [4, 8, 4, 8],
    "Phòng 4": [0, 4, 4, 8],
}

BEACON_CONFIG = {
    "DHT20_1": {"x": 0.0, "y": 7.0},   # Node 1 (góc trái-trên, Phòng 4)
    "DHT20_2": {"x": 8.0, "y": 7.0},   # Node 2 (góc phải-trên, Phòng 3)
    "DHT20_3": {"x": 8.0, "y": 0.0},   # Node 3 (góc phải-dưới, Phòng 2)
}

# ==========================================
# 2. CALIBRATION RSSI (QUAN TRỌNG!)
# ==========================================

# Đo RSSI khi đứng cách beacon 0.5m (hoặc 1 gang tay)
# VD: Nếu RSSI = -50dBm ở 0.5m → TX_POWER = -50
TX_POWER = -50  # ← THỬ GIẢM XUỐNG -50 hoặc -45

# Path loss exponent (môi trường trong nhà hẹp)
N_ENV = 1.8  # Giảm xuống 1.8 để nhạy hơn với thay đổi RSSI

SCAN_DURATION = 3.0
MIN_RSSI = -80  # Tín hiệu tối thiểu
MAX_DISTANCE_UNITS = 15  # 15 đơn vị = 3m thực (quá xa)

# ==========================================
# 3. RSSI → KHOẢNG CÁCH (ĐƠN VỊ MA TRẬN)
# ==========================================

def rssi_to_distance(rssi):
    """
    Chuyển RSSI sang khoảng cách THEO ĐỢN VỊ MA TRẬN (0-8)
    
    Công thức: d(m) = 10^((TX_POWER - RSSI) / (10*N))
    Sau đó: d_units = d(m) / REAL_SCALE
    """
    if rssi < -95:
        return 20.0  # Quá xa
    
    # Tính khoảng cách thực (mét)
    dist_meters = 10 ** ((TX_POWER - rssi) / (10 * N_ENV))
    
    # Chuyển sang đơn vị ma trận
    dist_units = dist_meters / REAL_SCALE
    
    return dist_units

# ==========================================
# 4. BẢNG TRA RSSI (TÙY CHỌN - ĐỂ KIỂM TRA)
# ==========================================

def create_rssi_lookup():
    """Tạo bảng tra để kiểm tra calibration"""
    print("\n--- BẢNG TRA RSSI → KHOẢNG CÁCH ---")
    print(f"TX_POWER={TX_POWER}, N_ENV={N_ENV}, Scale={REAL_SCALE}m/đơn vị")
    print("-" * 50)
    for rssi in range(-40, -81, -5):
        dist_units = rssi_to_distance(rssi)
        dist_meters = dist_units * REAL_SCALE
        print(f"RSSI: {rssi:3d}dBm → {dist_units:5.1f} đơn vị ({dist_meters:.2f}m thực)")
    print("-" * 50 + "\n")

# ==========================================
# 5. THUẬT TOÁN ĐỊNH VỊ
# ==========================================

def trilateration(beacons):
    """Trilateration chuẩn"""
    if len(beacons) < 3:
        return None
    
    b1, b2, b3 = beacons[:3]
    
    x1, y1 = BEACON_CONFIG[b1['name']]['x'], BEACON_CONFIG[b1['name']]['y']
    x2, y2 = BEACON_CONFIG[b2['name']]['x'], BEACON_CONFIG[b2['name']]['y']
    x3, y3 = BEACON_CONFIG[b3['name']]['x'], BEACON_CONFIG[b3['name']]['y']
    
    r1, r2, r3 = b1['distance'], b2['distance'], b3['distance']
    
    A = 2 * (x2 - x1)
    B = 2 * (y2 - y1)
    C = r1**2 - r2**2 - x1**2 + x2**2 - y1**2 + y2**2
    D = 2 * (x3 - x2)
    E = 2 * (y3 - y2)
    F = r2**2 - r3**2 - x2**2 + x3**2 - y2**2 + y3**2
    
    denom = (E * A - B * D)
    if abs(denom) < 0.0001:
        return weighted_centroid(beacons)
    
    x = (C * E - F * B) / denom
    y = (C * D - A * F) / denom
    
    # Clip trong phạm vi
    x = np.clip(x, -1, 9)
    y = np.clip(y, -1, 9)
    
    return (x, y)

def weighted_centroid(beacons):
    """Weighted centroid với trọng số mũ cao"""
    x_sum = 0
    y_sum = 0
    total_weight = 0
    
    for b in beacons:
        name = b['name']
        coords = BEACON_CONFIG[name]
        dist = b['distance']
        
        # Trọng số tỷ lệ nghịch bình phương (beacon gần ảnh hưởng rất lớn)
        weight = 1.0 / (dist ** 3 + 0.01)  # Dùng mũ 3 để nhấn mạnh beacon gần
        
        x_sum += coords['x'] * weight
        y_sum += coords['y'] * weight
        total_weight += weight

    if total_weight == 0: 
        return None
    return (x_sum / total_weight, y_sum / total_weight)

def hybrid_positioning(beacons):
    """Hybrid: Ưu tiên beacon gần nhất"""
    if len(beacons) < 3:
        return None
    
    # Sắp xếp theo khoảng cách
    beacons_sorted = sorted(beacons, key=lambda x: x['distance'])
    
    # Nếu beacon gần nhất < 2 đơn vị (0.4m), ưu tiên nó
    if beacons_sorted[0]['distance'] < 2.0:
        closest = beacons_sorted[0]
        coords = BEACON_CONFIG[closest['name']]
        
        # Offset nhẹ từ beacon gần nhất
        pos_tri = trilateration(beacons_sorted)
        if pos_tri:
            # Kéo về beacon gần 80%
            x = 0.8 * coords['x'] + 0.2 * pos_tri[0]
            y = 0.8 * coords['y'] + 0.2 * pos_tri[1]
            return (x, y)
    
    # Trường hợp bình thường: hybrid
    pos_tri = trilateration(beacons_sorted)
    pos_wc = weighted_centroid(beacons_sorted)
    
    if pos_tri is None:
        return pos_wc
    if pos_wc is None:
        return pos_tri
    
    # 60% trilateration, 40% weighted
    x = 0.6 * pos_tri[0] + 0.4 * pos_wc[0]
    y = 0.6 * pos_tri[1] + 0.4 * pos_wc[1]
    
    return (x, y)

def get_room_name(x, y):
    for room_name, limits in ROOM_MAP.items():
        if limits[0] <= x <= limits[1] and limits[2] <= y <= limits[3]:
            return room_name
    return "Ngoài vùng"

# ==========================================
# 6. QUÉT BLE
# ==========================================

scanned_data = {}

def detection_callback(device, advertisement_data):
    name = device.name or advertisement_data.local_name
    
    if name and name in BEACON_CONFIG:
        rssi = advertisement_data.rssi
        
        if rssi < MIN_RSSI:
            return
        
        if name not in scanned_data:
            scanned_data[name] = {"rssi_list": [], "info": "N/A"}
        
        scanned_data[name]["rssi_list"].append(rssi)
        
        manuf_data = advertisement_data.manufacturer_data
        if manuf_data:
            for _, data in manuf_data.items():
                if len(data) == 8:
                    try:
                        _, raw_temp, raw_hum = struct.unpack('<Ihh', data)
                        scanned_data[name]["info"] = f"{raw_temp/100:.1f}°C | {raw_hum/100:.1f}%"
                    except:
                        pass

async def scan_cycle():
    global scanned_data
    scanned_data = {}
    
    print(f"\r>>> Đang quét ({SCAN_DURATION}s)...    ", end="", flush=True)
    
    scanner = BleakScanner(detection_callback=detection_callback)
    await scanner.start()
    await asyncio.sleep(SCAN_DURATION)
    await scanner.stop()
    
    final_results = []
    
    for name, data in scanned_data.items():
        rssi_list = data["rssi_list"]
        count = len(rssi_list)
        
        if count > 0:
            # Dùng median
            avg_rssi = np.median(rssi_list)
            distance = rssi_to_distance(avg_rssi)
            
            if distance > MAX_DISTANCE_UNITS:
                continue
            
            final_results.append({
                "name": name,
                "rssi": avg_rssi,
                "distance": distance,
                "info": data["info"]
            })
    
    final_results.sort(key=lambda x: x['distance'])
    return final_results

# ==========================================
# 7. VẼ BẢN ĐỒ
# ==========================================

def draw_map(beacons, user_x, user_y, current_room):
    plt.close('all') 
    
    fig, ax = plt.subplots(figsize=(10, 10))
    colors = ['#FFE0B2', '#C8E6C9', '#BBDEFB', '#E1BEE7']
    
    # Vẽ phòng
    for i, (r_name, limits) in enumerate(ROOM_MAP.items()):
        x_min, x_max, y_min, y_max = limits
        rect = patches.Rectangle((x_min, y_min), x_max-x_min, y_max-y_min, 
                               linewidth=2, edgecolor='#757575', facecolor=colors[i], alpha=0.4)
        ax.add_patch(rect)
        ax.text(x_min + (x_max-x_min)/2, y_min + (y_max-y_min)/2, r_name, 
                ha='center', va='center', fontsize=12, fontweight='bold', alpha=0.3)

    # Vẽ Nodes
    for b in beacons:
        name = b['name']
        coords = BEACON_CONFIG[name]
        ax.scatter(coords['x'], coords['y'], c='red', marker='s', s=150, zorder=5)
        circle = plt.Circle((coords['x'], coords['y']), b['distance'], 
                          color='red', fill=False, linestyle=':', alpha=0.5)
        ax.add_patch(circle)
        
        # Hiển thị khoảng cách cả đơn vị và mét
        dist_m = b['distance'] * REAL_SCALE
        label = f"{name}\n{b['rssi']:.0f}dBm\n{b['distance']:.1f}u ({dist_m:.2f}m)"
        ax.text(coords['x'], coords['y'] - 0.6, label, ha='center', fontsize=8, 
                bbox=dict(facecolor='white', alpha=0.7, edgecolor='none'))

    # Vẽ Người dùng
    ax.scatter(user_x, user_y, c='blue', marker='o', s=400, zorder=10, 
              edgecolors='white', linewidth=3)
    ax.add_patch(plt.Circle((user_x, user_y), 0.5, color='blue', alpha=0.2))
    
    # Tính tọa độ thực
    real_x = user_x * REAL_SCALE
    real_y = user_y * REAL_SCALE
    user_label = f"BẠN\n({user_x:.1f}, {user_y:.1f})\n[{real_x:.2f}m, {real_y:.2f}m]"
    ax.text(user_x, user_y + 0.6, user_label, ha='center', fontweight='bold', 
            color='blue', fontsize=9)

    ax.set_xlim(-1, 9)
    ax.set_ylim(-1, 9)
    ax.set_aspect('equal')
    ax.set_title(f"LIVE TRACKING: {current_room.upper()} | {time.strftime('%H:%M:%S')}", 
                fontsize=14, fontweight='bold')
    ax.set_xlabel(f'X (1 đơn vị = {REAL_SCALE}m thực)', fontsize=10)
    ax.set_ylabel(f'Y (1 đơn vị = {REAL_SCALE}m thực)', fontsize=10)
    
    current_dir = os.path.dirname(os.path.abspath(__file__))
    filename = os.path.join(current_dir, 'Pos_matrix.png')
    
    try:
        plt.savefig(filename, dpi=80, bbox_inches='tight')
    except Exception as e:
        print(f"Lỗi lưu ảnh: {e}")

# ==========================================
# 8. CHƯƠNG TRÌNH CHÍNH
# ==========================================

async def run_app():
    print("\033[2J\033[H")
    print("=" * 70)
    print("HỆ THỐNG ĐỊNH VỊ REAL-TIME - Calibrated cho không gian nhỏ")
    print("=" * 70)
    print(f"Tỷ lệ: 1 đơn vị ma trận = {REAL_SCALE}m thực tế")
    print(f"Khoảng cách Node: ~8 đơn vị = {8*REAL_SCALE}m = 2 gang tay")
    
    # Hiển thị bảng tra
    create_rssi_lookup()
    
    print("Nhấn Ctrl+C để dừng\n")

    try:
        while True:
            valid_beacons = await scan_cycle()
            
            print(f"\r{' ' * 70}\r", end="") 
            
            if len(valid_beacons) >= 3:
                pos = hybrid_positioning(valid_beacons)
                
                if pos:
                    user_x, user_y = pos
                    current_room = get_room_name(user_x, user_y)
                    
                    draw_map(valid_beacons, user_x, user_y, current_room)
                    
                    # DEBUG: In chi tiết từng beacon
                    print("\n" + "="*70)
                    for b in valid_beacons[:3]:
                        dist_m = b['distance'] * REAL_SCALE
                        print(f"  {b['name']}: RSSI={b['rssi']:.0f}dBm → {b['distance']:.2f}u ({dist_m:.2f}m)")
                    
                    # Log chi tiết
                    beacon_info = " | ".join([
                        f"{b['name']}: {b['distance']:.1f}u ({b['distance']*REAL_SCALE:.2f}m)" 
                        for b in valid_beacons[:3]
                    ])
                    real_x, real_y = user_x * REAL_SCALE, user_y * REAL_SCALE
                    log_line = (f"[{time.strftime('%H:%M:%S')}] {current_room:<10} "
                              f"({user_x:.2f}, {user_y:.2f}) = [{real_x:.2f}m, {real_y:.2f}m]")
                    print(f"  VỊ TRÍ: {log_line}")
                    print("="*70)
                    
                else:
                    print(f"[{time.strftime('%H:%M:%S')}] Lỗi tính toán.")
            else:
                print(f"[{time.strftime('%H:%M:%S')}] Yếu tín hiệu ({len(valid_beacons)} nodes).")

    except KeyboardInterrupt:
        print("\n\nĐã dừng chương trình.")
    except Exception as e:
        print(f"\nLỗi: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    try:
        asyncio.run(run_app())
    except KeyboardInterrupt:
        pass