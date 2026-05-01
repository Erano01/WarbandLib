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
- signatures/   : exe fingerprint, pattern scan, offset tablolari
- core/         : memory, PE, version detection, logging
- runtime/      : oyun ici hook ve bridge katmani
- sdk/          : mod yazarinin tuketecegi API
- tools/        : dumper, verifier, generator araclari
- examples/     : ornek modlar
- docs/         : kararlar, ABI notlari, destek politikasi

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
