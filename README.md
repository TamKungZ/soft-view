# Soft View

เบา, native 100%, ไม่มี sandbox, ทำงานได้ทั้ง X11 และ Wayland
ดูวิดีโอ, รูปภาพ, ฟังเพลง ด้วย engine เดียว (libmpv)

## สถาปัตยกรรม
- **SDL2** — window + input, auto-detect X11/Wayland
- **libmpv** (render API) — decode/render วิดีโอ, รูปภาพ, เสียง
- **Dear ImGui** — overlay control bar แบบเบา (ไม่มี widget tree ให้ init)

## Dependencies

### Debian / Ubuntu
```
sudo apt install build-essential cmake pkg-config libsdl2-dev libmpv-dev
```

### Fedora
```
sudo dnf install gcc-c++ cmake pkgconf-pkg-config SDL2-devel mpv-libs-devel
```

### Arch
```
sudo pacman -S base-devel cmake sdl2 mpv
```

## Build
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```
ได้ binary ที่ `build/soft-view` — รันตรงๆ ได้เลย ไม่ต้อง install อะไรเพิ่ม

## ใช้งาน
```
./soft-view /path/to/video.mp4
./soft-view /path/to/photo.jpg
./soft-view /path/to/song.mp3
```
หรือเปิดโปรแกรมเปล่าๆ แล้วลากไฟล์มาวางในหน้าต่าง

## คีย์ลัด
| ปุ่ม | การทำงาน |
|---|---|
| Space | Play / Pause |
| F | Fullscreen |
| Esc | ออก fullscreen |
| ← / → | Seek ถอย/เดินหน้า 5 วินาที |
| ขยับเมาส์ | โชว์ control bar (ซ่อนอัตโนมัติหลัง 2.5s ตอนกำลังเล่น) |

## Roadmap ที่ยังไม่ทำ (เพิ่มได้ทีหลัง)
- Playlist / sidebar (ตอนนี้ตั้งใจตัดออกเพื่อความเบา ตาม UI แบบ A ที่เลือก)
- Config file สำหรับ default volume, hwdec เป็นต้น
- .desktop file + icon สำหรับ integrate เข้า file manager / app launcher
- Package เป็น .deb/.rpm/PKGBUILD สำหรับแจกจ่าย
