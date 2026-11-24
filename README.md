# WarbandLib

Mount & Blade: Warband için açık kaynak bir plugin runtime'ı ve kararlı C ABI. Amaç, farklı kişilerin farklı derleyicilerle yazdığı modların aynı oyunda yan yana çalışabildiği, Minecraft/Forge benzeri bir modding ekosistemi kurmak. İlk büyük hedef, bunun üzerine kurulacak bir kampanya co-op modu.

- Tek bir C header (`sdk/include/warbandlib/warbandlib.h`), sürümlü ve geriye uyumlu
- Bağımlılık sırasıyla yüklenen plugin'ler, öncelikli event'ler ve paylaşımlı hook zincirleri
- Plugin'ler arası servisler (interface'ler) ve oyun içi menü/çizim API'si
- Linux'tan da Windows'tan da aynı şekilde derlenir

## Kurulum

Gereken: CMake ≥ 3.21. Linux'ta ayrıca clang-cl, lld, Ninja ve [xwin](https://github.com/Jake-Shadle/xwin) (`~/toolchains/xwin-x86`); Windows'ta Visual Studio 2022.

```bash
# Linux
cmake --preset build-release-clangcl-x86
cmake --build --preset build-release-clangcl-x86

# Windows
cmake --preset windows-msvc-x86
cmake --build --preset windows-msvc-x86-release
```

Çıktı `build/<preset>/bin/` altında: `warbandlib_runtime.dll`, `injector.exe` ve `plugins/`.

## Dokümantasyon

Kurulum ayrıntıları, mimari, plugin yazma ve API referansı dokümantasyon sitesinde:

```bash
cd docs
npm install
npm run dev
```

## Lisans

[LICENSE](LICENSE)
