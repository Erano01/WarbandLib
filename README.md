# WarbandLib

Mount and Blade: Warband icin reverse engineering tabanli, acik kaynak bir modlama API ve runtime altyapisi.

```text
Hedef
- KenshiLib ve REKenshi benzeri bir akis kurmak
- once veri modeli ve binary davranisini cikarmak
- sonra signature/offset katmani kurmak
- en son stabil mod API acmak

Klasorler
- re/           : reverse engineering notlari, type recovery, call graph
- runtime/      : oyun ici hook ve bridge katmani

- signatures/   : exe fingerprint, pattern scan, offset tablolari
- core/         : memory, PE, version detection, logging

- sdk/          : mod yazarinin tuketecegi API
- tools/        : dumper, verifier, generator araclari
- examples/     : ornek modlar
- docs/         : kararlar, ABI notlari, destek politikasi

Game Files (Linux): 
/home/erano/.local/share/Steam/steamapps/common/MountBlade Warband/

Plan
- ilk hedef tek Warband exe surumu ve singleplayer
- ilk public yuzey read-only olacak
- sonra kontrollu hook/event sistemi eklenecek
- coklu surum destegi veri modeli oturduktan sonra gelecek

Toolchain
- runtime tarafi once Windows x86 ve MSVC 2010 uyumlu dusunuluyor
- Linux tarafi analiz, generator ve tool gelistirme icin uygun
- C++ ABI sorun cikarirsa ortak katman C ABI olacak

Pratik karar
- oyuna en yakin katman Windows tarafinda dogrulanir
- Linux tarafinda asenkron gelistirme devam eder
- bu repo CMake ile MSVC odakli ama GCC/Clang ile tool gelistirmeye acik tutulur
```

## Local iterasyon akisi

Bu repo icinde hizli gelistirme dongusu icin `tools/dev-iteration.sh` scripti eklendi.

Ornek kullanim:

```bash
./tools/dev-iteration.sh configure
./tools/dev-iteration.sh build
./tools/dev-iteration.sh sync --dry-run
./tools/dev-iteration.sh launch
```

Tek komutta configure + build + sync:

```bash
./tools/dev-iteration.sh loop
```

Makineye ozel ayarlar icin environment variable kullan:

```bash
export WARBANDLIB_MOD_TARGET_DIR="$HOME/.local/share/Warband/Modules/MyModule"
export WARBANDLIB_GAME_LAUNCH_CMD="steam -applaunch 48700"
```

VS Code tasklari:

- `WarbandLib: Configure`
- `WarbandLib: Build`
- `WarbandLib: Sync Module`
- `WarbandLib: Iteration Loop`
- `WarbandLib: Launch Game`
