# Cloth Simulation - OpenGL

Mô phỏng vải sử dụng OpenGL với C++.

## 📁 Cấu trúc dự án

```
ClothSimulation/
├── src/
│   ├── core/           # Application, Window, Input
│   ├── renderer/       # Renderer, Shader, Camera, Texture
│   ├── physics/        # PhysicsWorld, Particle, Constraint
│   ├── cloth/          # Cloth, ClothMesh
│   └── utils/          # MathUtils, Timer
├── shaders/            # Shader programs
├── assets/             # Tài nguyên (textures, models)
├── Libraries/          # Third-party libraries
└── glad.c             # GLAD loader
```

## 🛠️ Yêu cầu

- Visual Studio 2022 hoặc mới hơn
- Windows 10/11
- OpenGL 4.6 compatible GPU

## 🚀 Chạy dự án

1. Mở `ClothSimulation.slnx` trong Visual Studio
2. Build solution (Ctrl+Shift+B)
3. Run (F5)

## 🎮 Điều khiển

| Phím | Chức năng |
|------|-----------|
| W/A/S/D hoặc Mũi tên | Di chuyển camera |
| Mouse (click + drag) | Xoay camera |
| Space | Reset vải |
| Escape | Thoát |

## 📚 Thư viện sử dụng

- **GLFW** - Quản lý window và input
- **GLAD** - OpenGL loader
- **GLM** - Thư viện toán học
- **STB_IMAGE** - Load textures

## 🔧 Kỹ thuật mô phỏng

- **Verlet Integration** - Tích phân vật lý
- **Constraint Solver** - Giải các ràng buộc
- **Structural Constraints** - Ràng buộc cấu trúc
- **Shear Constraints** - Ràng buộc cắt
- **Bend Constraints** - Ràng buộc uốn

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

## 📸 Hình ảnh

Dự án hiển thị một tấm vải được treo từ hai góc và mô phỏng chuyển động dưới tác dụng của trọng lực.

## 📄 License

MIT License
