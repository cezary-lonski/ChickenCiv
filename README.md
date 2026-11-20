# Chicken Civ

Turowa strategia heksowa w stylu Civilization 5 z kurczaczą cywilizacją (Unreal Engine 5.8, C++). Pełny plan i status: [PLAN.md](PLAN.md).

## Uruchomienie

Skrypty w `Tools\` ustawiają środowisko (UE na `D:\Gry\Epic Games\UE_5.8`):

```powershell
powershell -File Tools\build.ps1 -Target ChickenCivEditor   # kompilacja (edytor musi byc zamkniety przy Live Coding)
powershell -File Tools\run-tests.ps1                         # testy automatyczne
```

Gra bez edytora:

```powershell
UnrealEditor.exe ChickenCiv.uproject /Game/Maps/Main -game -windowed
```

Opcje URL mapy pomijają menu: `?Seed=7 ?Size=Small|Medium|Large ?Players=2..8 ?Difficulty=Easy|Normal|Hard ?NoMenu`.

## Sterowanie

| Klawisz | Działanie |
|---------|-----------|
| WASD / strzałki, kółko | kamera, zoom |
| LPM | wybierz jednostkę / miasto / heks (kolejne kliknięcie przełącza) |
| PPM | ruch; na wroga obok: atak (wypowiedzenie wojny, bitwa) |
| B | załóż miasto (osadnik) |
| 1-9, PgUp/PgDn | produkcja w mieście |
| T | panel technologii (cyfry wybierają badanie) |
| A / J / L | armia: załóż (Dowódca), dołącz, wypuść ostatnią jednostkę |
| K, Tab | pomiń jednostkę, następna wolna jednostka |
| Enter / Spacja | koniec tury |
| F5 / F9 | zapis / odczyt (niedostępne w LAN) |
| Esc | menu |

Bitwa: LPM ruch, atak lub umiejętność (medyk leczy sojusznika, wskrzeszacz klika zwłoki, miotacz celuje czarną dziurą na 2 tury), Spacja kończy turę jednostki, R automatycznie rozstrzyga, D laser Gwiazdy Śmierci.

## Tryby wieloosobowe

- **Hot seat**: w menu ustaw „Gracze ludzcy” na więcej niż 1; między turami ekran zasłania planszę.
- **LAN**: gospodarz wybiera „Hostuj grę LAN”, pozostali „Dołącz do gry LAN” i wpisują jego adres IP; gospodarz startuje grę. Każda maszyna liczy tę samą deterministyczną grę, a serwer rozsyła polecenia; po każdej turze porównywany jest hash stanu (ostrzeżenie w HUD przy rozjeździe). Strategia LLM jest w LAN wyłączona.

## AI i klucz API

Bez klucza AI gra klasycznie. Klucz (Claude API) można wkleić w menu (Ctrl+V) albo ustawić `ANTHROPIC_API_KEY`; trafia tylko do lokalnego `GameUserSettings.ini`, nigdy do zapisów gry.

## Narzędzia deweloperskie

`-AutoShot="plik.png" -AutoShotDelay=45 [-CamZoomSteps=6]` zrzut ekranu i wyjście; `-DebugBattle` gotowa bitwa; `-ShowMenu` wymusza menu; `-AutoLan -AutoLanTurns=10` automatyczny gracz do testu LAN (`/Game/Maps/Main?listen?Lobby` dla gospodarza, adres IP dla klienta).
