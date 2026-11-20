# Plan: Chicken Civ (Unreal Engine)

Strategia turowa na mapie heksagonalnej w stylu Civilization 5, w której grasz cywilizacją kurczaków.

## Założenia

- Strategiczna mapa heksagonalna, turowa, w stylu Civ 5.
- Osobna taktyczna bitwa na planszy heksowej w stylu Heroes of Might and Magic.
- 4 rasy kurczaków: czerwone, żółte, niebieskie, zielone.
- Armie: dowódca + do 10 jednostek zwykłych + do 10 specjalnych (w tym zastępca).
- Drzewko technologii od starożytności do kosmosu, z trzema ścieżkami.
- Zasób: nuggetsy (+5 za każdego zabitego kurczaka), główne źródło energii.
- Pozostałe surowce jak w Civ 5.
- Przeciwnicy sterowani przez AI (API key), tryby LAN i hot seat.

## Rasy

| Rasa | Atrybut |
|------|---------|
| Czerwone | Nauka x3 (mnożnik punktów nauki) |
| Żółte | Siła x2 |
| Niebieskie | Statki: szybkość x10, siła x3, koszt 5 razy niższy (ustalone) |
| Zielone | Jednostka specjalna niszcząca całą armię, działa tylko gdy ktoś atakuje miasto (obrona miasta; ustalone) |

## Armie i jednostki

- Dowódca zbiera jednostki w armię: do 10 zwykłych + do 10 specjalnych.
- Zastępca (jednostka specjalna): gdy dowódca zginie, armia się nie rozpada.
- Zabicie dowódcy bez zastępcy: armia się rozpada, jednostki zostają pojedynczo na różnych heksach.
- Jednostki zwykłe jak w Civ 5, z dodatkami (np. X-wingi i TIE).

Jednostki specjalne:
- **Medyk**: leczy jednostki.
- **Miotacz czarnej dziury** (zaawansowana technologia): celuje 2 tury, potem strzela. Czarna dziura niszczy każdą jednostkę w promieniu 2 heksów.
- **Wskrzeszacz**: 1 wskrzeszenie na grę.
- **Zastępca dowódcy**.
- Zielona jednostka specjalna.

## Drzewko technologii

Główne drzewko do kosmosu, potem trzy ścieżki:

1. **Statki**: lepsze statki, stacje kosmiczne, promy kosmiczne (wożą kurczaki na stacje), Gwiazda Śmierci (cud świata).
   - Gwiazda Śmierci: w bitwie laser natychmiast niszczy wybraną jednostkę. Koszt w nuggetsach.
2. **Lasery**: laserowy pistoleciarz.
   - Połączenie statków i laserów daje X-wingi i TIE (małe statki wojenne).
3. **Pierwiastki**: akcelerator cząstek (+10 nauki w każdym mieście, gdzie zbudowany).
   - Czarne dziury: miotacz czarnej dziury.
   - Teleportacja: teleporter w mieście, teleport między teleporterami.

**Zaawansowana technologia** (po obu ścieżkach kosmosu i pierwiastkach): droidy, kwas, droidy miotające meteoryty itd.

## Etapy

### Etap 0: Fundament
- Projekt UE na dysku D (wersja UE, C++ dla logiki, Blueprinty dla UI).
- Siatka heksów (współrzędne osiowe), wybór heksa, kamera.
- Generator mapy: rozmiary, teren, surowce.
- Pętla tur i `GameState` zapisywalny do struktury danych (podstawa pod zapis, LAN i AI).

### Etap 1: Rdzeń Civ
- Miasta: zakładanie, produkcja, rozrost, budynki.
- Surowce jak w Civ 5 plus nuggetsy.
- Zwykłe jednostki (osadnik, robotnik, wojownik, łucznik itd.).
- Mgła wojny i widoczność.

### Etap 2: Rasy
- 4 rasy jako dane (mnożniki, jednostka specjalna).
- Wybór rasy i koloru.

### Etap 3: Drzewko technologii
- Dane w DataTable.
- Główne drzewko, trzy ścieżki, zaawansowana technologia.
- Odblokowywanie jednostek, budynków i cudów.

### Etap 4: Armie na mapie
- Dowódca, grupowanie 10+10, zastępca.
- Rozpad armii po śmierci dowódcy.
- Medyk, wskrzeszacz, miotacz czarnej dziury.

### Etap 5: Bitwa taktyczna (HoMM)
- Plansza heksowa, kolejka inicjatywy, ruch i atak.
- Umiejętności: leczenie, wskrzeszenie, czarna dziura (2 tury celowania).
- Laser Gwiazdy Śmierci.
- Wynik wraca na mapę: straty, nuggetsy, rozpad armii.

### Etap 6: AI przeciwników
- Podejście hybrydowe: klasyczne AI dla ruchów i walki, LLM przez API key do strategii, dyplomacji i osobowości cywilizacji.
- Powód: LLM na każdą jednostkę w każdej turze byłby wolny i drogi.
- Klucz API wpisuje gracz w ustawieniach, przechowywany lokalnie, nigdy w plikach gry.
- AI może wypowiadać wojny, ruszać jednostkami i robić wszystko to, co gracz.
- Poziomy trudności.

### Etap 7: Multiplayer
- Hot seat (pierwszy).
- LAN przez replikację UE, serwer jako autorytet nad stanem gry.

### Etap 8: Menu i ustawienia
- Trudność, wielkość mapy, liczba cywilizacji, rasa i kolor kurczaka.
- Zapis i wczytanie.

### Etap 9: Oprawa i balans
- Modele, animacje, UI, dźwięk, testy.

## Kolejność prac

0 -> 1 -> 2 -> 3 (podstawa) -> 4 -> 5 -> 6 -> 8 -> 7 -> 9.
Start: mapa, miasta i tury.

## Status (stan na 2026-11-19)

| Etap | Stan | Uwagi |
|------|------|-------|
| 0 Fundament | zrobiony | siatka heksow, generator mapy, kamera, zapis JSON |
| 1 Rdzen Civ | zrobiony | miasta, produkcja, wzrost, mgla wojny, jednostki cywilne i wojskowe |
| 2 Rasy | zrobiony | mnozniki ras, wybor rasy i koloru w menu |
| 3 Technologie | zrobiony | 22 technologie, trzy sciezki po kosmosie, odblokowania jednostek, budynkow i cudu |
| 4 Armie | zrobiony | dowodca, 10 zwyklych + 10 specjalnych, zastepca, rozpad, auto-dolaczanie |
| 5 Bitwa taktyczna | zrobiony | plansza heksowa, inicjatywa, medyk, wskrzeszacz, czarna dziura, laser Gwiazdy Smierci, zielony straznik |
| 6 AI | zrobiony | klasyczne AI (badania, produkcja, osadnicy, armie, wojna) + opcjonalna strategia LLM |
| 8 Menu i ustawienia | zrobiony | rozmiar mapy, liczba cywilizacji, trudnosc, rasa, kolor, ziarno, klucz API, zapis/odczyt |
| 7 Multiplayer | zrobiony | hot seat (ekran przekazania) i LAN (deterministyczna replikacja polecen, kontrola hasha) |
| 9 Oprawa i balans | w toku | patrz nizej |

### Etap 9: co jest, czego brakuje
Jest: ulepszenia pol (farma, kopalnia; klawisz I, AI tez ich uzywa), plynny ruch jednostek, wlasna siatka proceduralna dla jednostek (kurczaki z akcesoriami, roboty, statki, statki kosmiczne), miast i surowcow; 23 testy automatyczne (reguly, bitwa, AI, symulacja pelnej gry, determinizm).
Brakuje (potrzebne zasoby lub decyzje):
- Prawdziwe modele i animacje kurczakow (np. z Fab) oraz dzwiek i muzyka.
- Poprawki balansu po grze z ludzmi (koszty, sila jednostek, tempo nauki).
- Dyplomacja inna niz wojna (pokoj, sojusze); wiecej ulepszen pol (drogi).
- AI nie uzywa statkow ani lotnictwa; w bitwie z AI-obronca obronca jest zawsze sterowany przez AI.
- Walka morska i powietrzna na mapie (jednostki istnieja, ale bitwy rozgrywaja sie tylko ladem).

## Decyzje (dawne pytania)

1. Folder projektu: D:\Gry\ChickenCiv (nowy).
2. Unreal Engine 5.8, C++ dla logiki, interfejs rysowany przez HUD.
3. 3D z kamera z gory.
4. LLM dla AI: Claude (API Anthropic), model domyslny claude-haiku-5-5, zmieniany zmienna CHICKEN_LLM_MODEL. Klucz tylko lokalnie (ANTHROPIC_API_KEY albo menu).
5. Niebieskie: statki szybkosc x10, sila x3, koszt 5 razy nizszy.
6. Zielona jednostka: niszczy cala armie atakujaca miasto (tylko w obronie miasta).
7. Dodatkowe jednostki i cuda: lista w kodzie (Model/CivState.h, Model/TechTree.cpp).
