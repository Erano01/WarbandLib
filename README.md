# WarbandLib

Mount & Blade: Warband için reverse engineering tabanlı, açık kaynak bir modding çekirdeği. Oyunun resmi bir modding API'si yok — WarbandLib bu boşluğu dolduran alt seviye katman: injection, hooking, struct/offset recovery.

Bu repo yalnızca oyuna en yakın C++ katmanını kapsar. Üstüne kurulacak yüksek seviye API (WarbandAPI, ayrı repo, Java) için entegrasyon şekli henüz kararlaştırılmadı. WarbandLib bu karardan bağımsız, kendi başına derlenip test edilebilir şekilde geliştiriliyor.

Emsal projeler: KenshiLib/RE_Kenshi, Warband Script Extender (WSE).

## Hedef
- Tek bir Warband exe sürümü, tek hedef: Windows build (Proton üzerinden Linux'ta çalıştırılıp test ediliyor).
- İlk yüzey read-only — hook kurup gözlemlemek. Kontrollü event/hook sistemi bundan sonra gelir.
- Çoklu sürüm desteği, veri modeli oturduktan sonra.

## Yerel ortam (Arch Linux + Proton)

| Ne | Değer |
|---|---|
| Oyun kurulum dizini | `/home/{user}/.local/share/Steam/steamapps/common/MountBlade Warband/` |
| Hedef exe | `.../MountBlade Warband/mb_warband.exe` |
| Steam App ID | `48700` |
| Proton prefix | `~/.local/share/Steam/steamapps/compatdata/48700/pfx` |
| Compatibility tool | Proton Experimental — Steam > Warband > Özellikler > Uyumluluk'tan zorlanmalı |
| Oyunu başlatma | `steam -applaunch 48700` |
| Module klasörleri | `.../MountBlade Warband/Modules/<ModAdı>/` |

Not: Bu oyunun resmi bir native Linux build'i de var (Steam Linux Runtime/scout ile gelir, varsayılan kurulum bu). Windows build'ini (`mb_warband.exe`) almak için compatibility tool Proton Experimental'a zorlanıp oyun yeniden indirilmeli — WarbandLib native Linux build'i değil, Windows build'i hedefliyor.

## Toolchain
- **Build**: `i686-w64-mingw32-gcc` (mingw-w64) ile Arch Linux'tan doğrudan cross-compile. Windows makine ya da VM gerekmiyor.
- **Fallback**: `clang -target i686-pc-windows-msvc -fms-compatibility-version=16.00` (ince ABI uyuşmazlığı çıkarsa).
- Gerçek MSVC 2010'a ihtiyaç yok: WarbandLib oyunun derlenmiş objeleriyle statik link etmiyor, struct/vtable erişimi RE ile çıkarılan offsetlerle elle yapılıyor — bu compiler-agnostic bir yaklaşım.
- **Çalıştırma/test**: gerçek Windows exe, Proton Experimental ile aynı Linux makinede. Windows VM günlük döngüde gerekmiyor; sadece nadiren "gerçek Windows'ta da doğrula" sanity-check'i için opsiyonel yedek.
- **RE araçları**: Ghidra (static analysis), x64dbg (Wine altında, dynamic debugging), frida (runtime tracing/prototipleme).
- **Test framework**: gtest — oyun çalışmadan test edilebilen saf mantık için (AOB scanner, PE parser). Gerçek hook/offset doğrulaması sadece canlı oyuna karşı yapılabilir.

## Klasörler
- `re/` — RE bulguları: struct/type recovery notları, call graph, Ghidra çıktıları.
- `signatures/` — AOB pattern verisi + exe fingerprint → offset tablosu.
- `core/` — memory read/write/protect, PE parsing, versiyon tespiti, AOB scan engine, logging.
- `runtime/` — `DllMain`, hook motoru (trampoline/detour), somut hook'lar.
- `sdk/` — Ham hook'ları event'e çeviren katman. Üst katman (dış API entegrasyonu) netleşmeden dokunulmuyor.
- `tools/` — dumper, signature verifier, `dev-iteration.sh`.
- `examples/` — ham hook örnekleri, WarbandLib'in kendi doğrulaması için.
- `docs/` — ABI notları, kararlar, destek politikası.

## Şu anki milestone
Injection → module base resolve → tek bilinen fonksiyona AOB scan → trampoline hook kur → tetiklenince log satırı yaz → temiz unhook.

Açık karar: injection mekanizması henüz belirlenmedi (bağımsız injector mı yazılacak, mevcut bir loader'ın üstüne mi kurulacak).

## Local iterasyon akışı

Hızlı geliştirme döngüsü için `tools/dev-iteration.sh`:

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

Ortam değişkenleri:

```bash
export WARBANDLIB_MOD_TARGET_DIR="$HOME/.local/share/Steam/steamapps/common/MountBlade Warband/Modules/MyModule"
export WARBANDLIB_GAME_LAUNCH_CMD="steam -applaunch 48700"
```

VS Code tasklari:

- `WarbandLib: Configure`
- `WarbandLib: Build`
- `WarbandLib: Sync Module`
- `WarbandLib: Iteration Loop`
- `WarbandLib: Launch Game`

## Build ve Inject Komutları

```bash
export GAME_DIR="$HOME/.local/share/Steam/steamapps/common/MountBlade Warband"
export WINE_DIR="$HOME/.local/share/Steam/steamapps/common/Proton - Experimental/files/bin"
export WINEPREFIX="$HOME/.local/share/Steam/steamapps/compatdata/48700/pfx"

# build
cmake -S . -B build-win --toolchain cmake/toolchain-mingw-i686.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-win -j

# deploy
cp build-win/warbandlib_injector.exe build-win/*.dll build-win/mb_warband.ini "$GAME_DIR/"

# oyunu başlat (pencere gelene kadar bekle, sonra devam et)
steam -applaunch 48700
```

```bash
# inject (oyun açık olmalı, her satır bağımsız bir DLL)
cd "$GAME_DIR"
"$WINE_DIR/wine" warbandlib_injector.exe mb_warband.exe warbandlib_runtime.dll
"$WINE_DIR/wine" warbandlib_injector.exe mb_warband.exe warbandlib_example_overlay_quad.dll
"$WINE_DIR/wine" warbandlib_injector.exe mb_warband.exe warbandlib_example_imgui_menu.dll
```

```bash
# log (her satırı ayrı terminalde çalıştır, ikisi de foreground'da bloke eder)
tail -f "$GAME_DIR/WarbandLib.log"
tail -f "$GAME_DIR/WarbandLibExampleImguiMenu.log"
```

```bash
# eject: test bitince ya da yeni build'i tekrar yüklemeden önce
"$WINE_DIR/wine" warbandlib_injector.exe --eject mb_warband.exe warbandlib_example_overlay_quad.dll
"$WINE_DIR/wine" warbandlib_injector.exe --eject mb_warband.exe warbandlib_example_imgui_menu.dll
```
