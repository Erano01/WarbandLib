# mb_warband.exe — recovered addresses

Fingerprint (SHA-256): `ff4a28c51bed49fe4bbb4bb96389f7611894f6d031bf68fff661efdc946dae13`
Image base at analysis time: `0x00400000` (non-ASLR)

| Name | Address | Type |
|---|---|---|
| `CD3DApplication_Initialize3DEnvironment` | `0x00402230` | function |
| `CD3DApplication_HandleDeviceLost` | `0x00402790` | function |
| `CMyD3DApplication::vftable` | `0x00816A14` | vtable |
| `g_pMyD3DApplication` | `0x02DAB810` | global (`CMyD3DApplication` instance) |
| `g_pMyD3DApplication + 0xC` | `0x02DAB81C` | field (`IDirect3DDevice9*`) |

`IDirect3DDevice9` vtable: `EndScene` = index 42 (offset `0xA8`), `Present` = index 17 (offset `0x44`), `Reset` = index 16 (offset `0x40`), `GetCreationParameters` = index 9 (offset `0x24`).
