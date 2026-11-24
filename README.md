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
- **Linux'tan**: clang-cl + lld-link, hedef `i686-pc-windows-msvc`, Windows SDK/CRT xwin ile (`~/toolchains/xwin-x86`). Preset: `build-release-clangcl-x86`.
- **Windows'tan**: Visual Studio 2022 (MSVC ya da ClangCL toolset). Preset: `windows-msvc-x86`, `windows-clangcl-x86`.
- Bağımlılıklar CMake FetchContent ile: ImGui v1.60, MinHook v1.3.4 (testler için GoogleTest, sistemde yoksa).
- Unit testler native: `linux-host-tests` preset'i.
- Plugin arayüzü sadece C ABI (`sdk/include/warbandlib/warbandlib.h`), bu yüzden plugin'ler hangi derleyiciyle derlendiğinden bağımsız.

```bash
cmake --preset build-release-clangcl-x86 && cmake --build --preset build-release-clangcl-x86
tools/hot-reload.sh     # oyuna enjekte et (protonhax), varsa eski kopyayı kaldır
tools/unload.sh         # temiz kaldır
```

Dokümantasyon: `docs/` (Vue + Vite) — `cd docs && npm install && npm run dev`.

## Klasörler
- `re/` — RE bulguları: struct/type recovery notları, call graph, Ghidra çıktıları.
- `signatures/` — AOB pattern verisi + exe fingerprint → offset tablosu.
- `core/` — memory read/write/protect, PE parsing, versiyon tespiti, AOB scan engine, logging.
- `runtime/` — `DllMain`, plugin loader, event bus, hook zincirleri, overlay (`host/` platformdan bağımsız, `platform/` Win32).
- `sdk/` — Public C ABI (`warbandlib.h`) ve x86 layout kilidi (`abi_lock.c`).
- `tools/` — dumper, signature verifier, `dev-iteration.sh`.
- `examples/` — ham hook örnekleri, WarbandLib'in kendi doğrulaması için.
- `docs/` — Vue + Vite dokümantasyon sitesi.

## Durum
Runtime 0.1 / ABI 1.0: plugin loader (bağımlılık + sıralama), öncelikli event bus, plugin'ler arası interface'ler, paylaşımlı öncelikli hook zincirleri, güvenli bellek erişimi, ImGui 1.60 overlay (INSERT) ve C menü/çizim API'si.

Sıradaki: offset'leri repoya koymadan, geliştiricinin kendi exe'sinden yerel üretilen sembol tablosu (bkz. docs → "Game data model").

## Local iterasyon akışı

`tools/dev-iteration.sh configure|build|test|inject|unload|loop|docs`
