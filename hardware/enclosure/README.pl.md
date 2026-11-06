# Obudowa whisli bean

**[English version](README.md)**

**33 × 66 × 10,4 mm.** Na tyle płaska, żeby wchodziła pod wzmacniacz, a magnesy pozwalają przyczepić ją do stali, np. do boku urządzenia. Góra i boki to jedna bryła. Podział i wycięcia na paznokieć są tylko od spodu. Z przodu wystaje soczewka odbiornika, z tyłu jest USB-C.

## Pliki

| Plik | Co to jest |
|---|---|
| `print/korpus.stl`, `print/dno.stl` | korpus i dno, już ułożone do druku |
| `print/whisli-bean-obudowa.step` | model złożony z makietą elektroniki i magnesów |
| `preview/*.png` | podglądy i przekrój; `gora.png` pokazuje układ, a `napis.png` napis |

## Jak to działa

- **Dno:** cztery zaczepy, po dwa na bok. Każdy to belka 1,1 × 25,5 mm zamocowana na obu końcach, z zaczepem 4 mm pośrodku, uniesiona 1,2 mm nad pełnym dnem. Ugina się w płaszczyźnie warstw, czyli w mocnym kierunku druku. Ugięcie 0,3 mm daje z obliczeń ok. 0,7% odkształcenia. Belka drukuje się jako most między słupkami, bez podpór.
- **Otwieranie:** wycięcie na paznokieć jest na obu długich bokach, od spodu. Paznokieć wchodzi od dołu i zahacza o rowek w krawędzi dna. Siła zdejmowania to szacunkowo ok. 16 N przy równym ciągnięciu.
- **Płytka:** wchodzi na lekki zatrzask. Z każdej strony jest cienka belka w korpusie (0,8 × 18 mm) z ząbkiem 0,25 mm. Skos ząbka działa jak klin: dociska płytkę do klocków w korpusie niezależnie od rzeczywistej grubości laminatu. Każdy przedni róg obejmuje pełny klocek, a małe żebro na nim kasuje luz wzdłuż płytki. Siła wyjmowania to szacunkowo ok. 6 N. Po zamknięciu dno dodatkowo podpiera płytkę szynami.
- **Odbiornik:** korpus siedzi w kanale grubej przedniej ściany, a soczewka wychodzi przez okrągły otwór. Z tyłu blokuje go pełny klocek 3 mm na dnie, ze skosem, który dosuwa odbiornik przy zamykaniu.
- **Magnesy:** dwa 20 × 10 × 2 mm w gniazdach 22 × 12 × 1 mm (po 1 mm zapasu z każdej strony), między płytką a odbiornikiem. Pod magnesem zostaje 0,6 mm dna, żeby trzymały jak najmocniej na stali.
- **Zdejmowanie z metalu:** zsuwaj obudowę po powierzchni. Magnesy trzymają mocniej niż zaczepy, więc przy ciągnięciu na wprost korpus może oderwać się od dna.
- **Bryła:** gniazdo USB i soczewka są w połowie wysokości. Ścianki boczne mają 3,5 mm, a dno pod płytką 1,2 mm.
- **Diody:** od środka okienko 1 × 4 mm, a na zewnątrz zostaje 0,4 mm plastiku. Kołnierz ogranicza mieszanie światła.
- **Napis:** „whis.li” na środku góry, Helvetica Neue, 19,5 mm długości, grawer 0,4 mm, okrągłe kropki. Czyta się go przy USB z lewej i soczewce z prawej. Litery są lekko rozstawione, zewnętrzna warstwa grawerunku jest o 0,06 mm szersza, a blok jest wyśrodkowany optycznie.
- **Antena:** ok. 10,4 mm wolnego miejsca od końca płytki do magnesów.

## Montaż

Bez goldpinów. Cienkie przewody (ok. 30 AWG, silikon), ok. 55 mm, lutowane wprost do otworów. Rekomenduję kabelki od spodu płytki, bo wtedy obudowa ładnie się zamyka.

| TSOP34438 | ESP32-C3 Super Mini |
|---|---|
| 1 OUT | GPIO4 |
| 2 GND | G |
| 3 VS | 3.3 |

1. Wklej magnesy w gniazda na dnie. Najprościej kupić je z taśmą 3M (Amazon i inne portale). Bez taśmy użyj kleju epoksydowego lub cyjanoakrylowego i odczekaj do pełnego utwardzenia. Gdy możesz wybrać, ułóż je przeciwnymi biegunami w dół: przyciągają się wtedy, zamiast odpychać, a na stali trzymają zwykle mocniej.
2. Nóżki odbiornika skróć do ok. 3 mm i odegnij do tyłu.
3. Odwróć korpus dnem do góry. Wsuń odbiornik w kanał, aż soczewka wyjdzie przez otwór, nóżkami w stronę pinów 5V, G, 3.3, GPIO4.
4. Włóż płytkę elementami w dół, pod małym kątem: najpierw gniazdo USB w otwór, potem dociśnij przód do kliknięcia. Wyjmuj odwrotnie: podważ przedni brzeg na środku, między klockami.
5. Przewody ułóż wzdłuż ścianki po stronie tych pinów (patrząc z góry, USB do siebie: lewa strona), nad belkami.
6. Wciśnij dno aż do kliknięcia zaczepów.

## Druk

- **Biały lub jasny materiał**, bo diody mają prześwitywać. Polecany ASA, ewentualnie matowy PETG. Przy ciepłym wzmacniaczu nie używaj PLA, bo mięknie przy ok. 55–60°C.
- **STL są już ułożone zewnętrzną stroną na stole, nie obracaj ich w slicerze.**
- Bez podpór. Belki zaczepów drukują się jako mosty: po wydruku sprawdź, czy nie przykleiły się do dna. Warstwa 0,2 mm.
- Okienka i napis to dwie pierwsze warstwy korpusu, więc pierwsza warstwa musi być równa, a kompensacja „stopy słonia” włączona (ok. 0,1 mm). Cienkie litery mogą się przy zbyt mocno dociśniętej warstwie zamknąć.
- **Dla najlepszego napisu:** gładka płyta, „Bottom surface pattern” ustawiony na „Monotonic line”, wolna pierwsza warstwa (ok. 30 mm/s), przepływ pierwszej warstwy nie wyższy niż 100%, płyta odtłuszczona.
- Szew ustaw z tyłu, po stronie USB. Gładka płyta daje gładki wierzch.

## Zmierz przed drukiem

Wymiary odbiornika są z karty katalogowej. Płytka jest z rysunku wymiarowego, a Super Mini różnią się między producentami.

| Wymiar | Co zmierzyć | Założone |
|---|---|---|
| Wystawanie USB | wystawanie gniazda poza krawędź płytki; **najważniejsze: gniazdo kończy się równo ze ścianą** | 1,4 mm |
| Laminat i gniazdo | grubość laminatu, wysokość gniazda | 1,2 mm, 3,2 mm |
| Płytka | szerokość i długość | 18 mm, 22,5 mm |
| Diody | środek diody: X od środka płytki przy USB skierowanym od siebie, Y od krawędzi z USB | PWR (−6; 5,3), IO8 (4,5; 11,2) |

## Sprawdź po wydruku

1. Pod belkami dna jest prześwit na całej długości, a dno wchodzi bez siłowania.
2. Dno klika, trzyma i schodzi paznokciem od spodu. Zaczep wystaje 0,65 mm.
3. Płytka klika i trzyma się sztywno. Ząbek wystaje 0,25 mm.
4. Wtyk USB-C wchodzi do końca.
5. Światło diod przechodzi, a diody są na środku okienek.
6. Sygnał Wi-Fi jest w porządku po zamknięciu z magnesami i na metalu.
7. Obudowa zsuwa się z metalu bez odrywania dna.

## Założenia

- **Dno zamiast pokrywy:** podział jest na spodzie, a góra i boki są jednolite. Wycięcia na paznokieć też są na spodzie.
- **Układ:** magnesy nie mieszczą się obok ani pod płytką bez podnoszenia całej bryły, więc obudowa jest dłuższa, a niższa. Stoją tak daleko od anteny, jak pozwala proporcja 1:2.
- **BOOT i RESET bez otworów:** reset hasła (BOOT 10 s) wymaga zdjęcia dna.
