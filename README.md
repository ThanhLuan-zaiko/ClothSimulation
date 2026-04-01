# Cloth Simulation - OpenGL

Mô phỏng vải 3D sử dụng OpenGL Compute Shader với vật lý GPU-based, hỗ trợ **15 màu debug visualization** để phát hiện bug va chạm.

## 📁 Cấu trúc dự án

```
ClothSimulation/
├── src/
│   ├── core/           # Application, Window, Input
│   ├── renderer/       # Renderer, Shader, Camera, Texture
│   ├── physics/        # GPUPhysicsWorld, Particle, Constraint
│   ├── cloth/          # Cloth, ClothMesh
│   └── utils/          # MathUtils, Timer, GPUDetection
├── shaders/
│   ├── cloth/          # Cloth rendering shaders
│   ├── physics/modular/ # GPU physics compute shaders
│   ├── terrain/        # Terrain rendering
│   ├── sphere/         # Mirror sphere
│   └── postprocess/    # SSAO, blur
├── assets/             # Tài nguyên (textures, models)
├── Libraries/          # Third-party libraries
└── glad.c             # GLAD loader
```

## 🛠️ Yêu cầu

- Visual Studio 2022 hoặc mới hơn
- Windows 10/11
- OpenGL 4.6 compatible GPU
- GPU hỗ trợ Compute Shader và SSBO

## 🚀 Chạy dự án

1. Mở `ClothSimulation.slnx` trong Visual Studio
2. Build solution (Ctrl+Shift+B)
3. Run (F5)

## 🎮 Điều khiển

### Camera & Tương tác

| Phím | Chức năng |
|------|-----------|
| W/A/S/D | Di chuyển camera |
| Q/Ctrl | Hạ xuống/Nâng lên |
| Mouse (click + drag) | Xoay camera |
| E | Tương tác vải (kéo/vortex) |
| Space | Reset vải |
| H | Reset camera |
| Escape | Thoát |

### Debug Visualization (MỚI! 🔍)

| Phím | Chức năng |
|------|-----------|
| **INSERT** | **Bật/tắt debug visualization** |
| **DELETE** | **Cycle qua các debug modes** |
| **F1-F10** | **Chọn mode debug cụ thể** |

### Điều khiển khác

| Phím | Chức năng |
|------|-----------|
| T | Đổi texture terrain |
| Shift+T | Lùi texture terrain |
| 1-9 | Chọn nhanh texture terrain |
| F | Toggle wireframe terrain |

## 🎨 Debug Color System - 15 Màu Phát Hiện Bug

Hệ thống debug visualization giúp xác định nguyên nhân vải **xuyên qua sphere** và **nổ khi va terrain**.

### Màu sắc & Ý nghĩa

| # | Màu | RGB | Bug Category | Khi nào xuất hiện |
|---|-----|-----|--------------|-------------------|
| 1 | ⬜ **Trắng** | (1,1,1) | Normal | Không có vấn đề |
| 2 | 🟥 **Đỏ** | (1,0,0) | Sphere CCD Failure | Xuyên qua sphere không phát hiện |
| 3 | 🟧 **Cam** | (1,0.5,0) | Sphere Margin | Vào vùng margin sphere (2.5m) |
| 4 | 🟨 **Vàng** | (1,1,0) | Terrain CCD Failure | Xuyên qua terrain không phát hiện |
| 5 | 🟩 **Xanh lá** | (0,1,0) | Terrain Margin | Vào vùng margin terrain (1m) |
| 6 | 🟦 **Cyan** | (0,1,1) | Self-Collision Failure | Hạt tự đâm vào nhau |
| 7 | 🔵 **Xanh dương** | (0,0,1) | Constraint Stretch | Constraint giãn > 5% |
| 8 | 🟣 **Magenta** | (1,0,1) | Shear Failure | Shear constraint > 10% |
| 9 | 🟪 **Tím** | (0.5,0,0.5) | Bending Limit | Góc uốn > 144° |
| 10 | 🩷 **Hồng nhạt** | (1,0.5,0.5) | Strain Rate | Biến dạng > 5%/frame |
| 11 | 🟤 **Nâu** | (0.6,0.3,0) | Velocity Explosion ⚠️ | Vận tốc quá lớn (nguyên nhân nổ) |
| 12 | 🟥 **Đỏ đậm** | (0.5,0,0) | Deep Penetration | Xuyên sâu > 0.5m |
| 13 | 🟩 **Xanh đậm** | (0,0.5,0) | Buffer Sync | Đồng bộ buffer có vấn đề |
| 14 | 🔵 **Xanh dương đậm** | (0,0,0.5) | Constraint Starvation | Thiếu constraints |
| 15 | ⬛ **Xám** | (0.5,0.5,0.5) | Sleeping | Hạt đang ngủ/jitter |

### 🐛 Debug Workflow - Tìm Bug Vải Xuyên Sphere

1. **Nhấn INSERT** để bật debug mode
2. **Quan sát 3 tấm vải** khi rơi:
   - **Bình thường**: Toàn bộ vải màu **trắng**
   - **Đến gần sphere**: Xuất hiện màu **cam** (margin detection hoạt động)
   - **Va chạm sphere**: Vẫn thấy cam hoặc về **trắng** (va chạm thành công)
   - **XUYÊN QUA SPHERE**: Xuất hiện màu **đỏ** (CCD failure - đây là bug!)
   - **Rơi xuống terrain**: Xuất hiện màu **vàng** hoặc **nâu** (velocity explosion)

3. **Phân tích màu sắc**:
   - Nếu thấy **đỏ**: CCD binary search TOI không tìm thấy điểm va chạm
   - Nếu thấy **nâu**: Velocity quá lớn gây nổ (cần giảm maxVelocity)
   - Nếu thấy **xanh dương**: Constraints bị over-stretch (cần tăng iterations)
   - Nếu thấy **cyan**: Self-collision không hoạt động (cần tăng radius)

4. **Nhấn DELETE** để cycle qua các mode nếu cần góc nhìn khác

### 🎯 Expected Behavior (Khi Fix Xong)

```
Frame 0-60:   Vải rơi tự do → MÀU TRẮNG
Frame 60-90:  Đến gần sphere → VIỀN CAM (margin detection)
Frame 90-120: Va chạm sphere → CAM/TRẮNG (draping nhẹ nhàng)
Frame 120+:   Rơi xuống terrain → XANH LÁ → TRẮNG (stable)
```

### 🔧 Các File Debug Shader Chính

```
shaders/physics/modular/
├── physics_common.glsl        # 15 color constants & helpers
├── physics_ccd_solve.comp     # Red/Orange - Sphere CCD debug
├── physics_collide.comp       # Yellow/Lime/Brown - Terrain & velocity
├── physics_solve.comp         # Blue/Magenta - Constraint stress
├── physics_bend.comp          # Purple - Bending limits
├── physics_strain_limit.comp  # Pink - Strain rate violations
├── physics_resolve.comp       # Cyan - Self-collision failures
└── physics_apply_deltas.comp  # Dark Green - Buffer sync issues
```

## 📚 Thư viện sử dụng

- **GLFW** - Quản lý window và input
- **GLAD** - OpenGL loader
- **GLM** - Thư viện toán học
- **STB_IMAGE** - Load textures

## 🔧 Kỹ thuật mô phỏng

### Vật lý GPU-based

- **Verlet Integration** - Tích phân vị trí (compute shader)
- **Constraint Solver** - Giải constraints bằng graph coloring
- **Spatial Hash Grid** - Broadphase collision detection
- **CCD (Continuous Collision Detection)** - Binary search Time of Impact
- **Strain Limiting** - Green-Lagrange strain constraint

### Các loại Constraints

| Loại | Stiffness | Mô tả |
|------|-----------|-------|
| Structural | 1.0 | Giữ khoảng cách hạt liền kề |
| Shear | 0.95 | Chống biến dạng cắt |
| Bend | 0.6 | Chống uốn cong (dihedral angle) |
| Skip-4 | 0.8 | Long-range constraints |
| Skip-8 | 0.64 | Ultra long-range (high-res only) |

### Collision Detection

- **Sphere Collision**: Ray-sphere intersection + binary search TOI
- **Terrain Collision**: Heightmap sampling + midpoint CCD
- **Self-Collision**: Analytic sphere-sphere CCD + multi-sample verification
- **Margins**: Sphere (2.5m), Terrain (1.0m) - phát hiện sớm

## 📝 Ghi chú cho Developer

### Thêm file mới vào project

1. Tạo file `.cpp` và `.h` trong thư mục thích hợp
2. Thêm vào `ClothSimulation.vcxproj` trong `<ItemGroup>`
3. Cập nhật `ClothSimulation.vcxproj.filters`

### Build shaders từ files

```cpp
Shader shader = Shader::CreateFromFile("shaders/cloth/cloth.vert",
                                        "shaders/cloth/cloth.frag");
```

### Debug Shader Development

```glsl
// Trong compute shader
#include "physics_common.glsl"

// Set debug color
setParticleColor(particleIdx, DEBUG_COLOR_SPHERE_CCD_FAIL);

// Trong fragment shader
uniform int u_DebugMode;  // 0 = off, 1 = show debug colors
if (u_DebugMode == 1) {
    vec3 debugColor = getDebugColor(v_State);
    out_Color = vec4(debugColor, 1.0);
}
```

## 📸 Hình ảnh

Dự án hiển thị 3 tấm vải với texture khác nhau rơi từ độ cao khác nhau, va chạm với sphere và terrain, sử dụng debug visualization để phát hiện bug.

## 📄 License

MIT License
