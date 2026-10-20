# PsyTunnel

Itsenäinen C++-tunnelidemo Psytriksen plasmatunnelin ajatuksesta. Ikkunan ja
näppäimistön hoitaa SDL2; tunnelin jokainen pikseli lasketaan OpenGL-shaderilla.
Psytriksen tapaiset aaltoilevat renkaat ja seinämien plasma saavat tässä aidon
perspektiivin: kamera liikkuu putken sisällä ja katsoo eri suuntaan kuin liikkuu.

## Käynnistys

Valmiiksi käännetty ohjelma:

```bash
cd ~/Code/PsyTunnel
./build/psytunnel --fullscreen
```

Oletuksena piirretään ikkunan tai näytön todelliseen pikselitarkkuuteen.
VSync tahdistaa päivityksen näytön virkistystaajuuteen, jos ajuri tukee sitä.
Esimerkiksi lukittu Full HD ja 156 FPS:n yläraja:

```bash
./build/psytunnel --fullscreen --resolution 1920x1080 --fps 156
```

`--resolution` määrää koko tunnelin piirtoresoluution sekä ikkunassa että koko
näytössä. Jos näyttö on eri kokoinen, valmis kuva sovitetaan siihen kuvasuhde
säilyttäen. Ilman valitsinta piirtoresoluutio seuraa ikkunan ja näytön kokoa.
FPS-raja on yläraja; saavutettu nopeus riippuu näytönohjaimesta ja tarkkuudesta.

## Näppäimet

| Näppäin | Toiminto |
| --- | --- |
| F11 | Koko näyttö / ikkuna |
| Esc | Lopeta |
| Space tai Pause | Pysäytä / jatka koko animaatiota |
| 0 | Automaattinen värikierto |
| 1–6 | Harmaasävyt, sateenkaari, sininen, vihreä, keltainen, punainen |
| Ylös / alas | Lisää / vähennä etenemisnopeutta |
| Vasen / oikea | Vähennä / lisää kameran huojuntaa |
| R | Vaihda etenemissuuntaa pehmeästi |
| F1 | Tulosta ohje terminaaliin |

Automaattinen värikierto käyttää yllä olevaa järjestystä. Kukin 14 sekunnin
jakso sisältää 10 sekuntia valittua palettia ja 4 sekunnin häivytyksen seuraavaan.
Numeronäppäin valitsee paletin heti. Huojuntaa ja etenemisnopeutta voi muuttaa
pelkän kameran liikkeen arvioimiseksi: `--sway 0` keskittää kameran, `--speed 0`
pysäyttää etenemisen, mutta jättää huojunnan ja plasman elämään.

```bash
./build/psytunnel --palette rainbow --sway 1 --speed 1.2
./build/psytunnel --palette grayscale --sway 0 --speed 0.8
```

## Kääntäminen

Tarvitaan C++17-kääntäjä, CMake, pkg-config, SDL2-kehitystiedostot ja OpenGL.
Toteutus käyttää Linuxin OpenGL-funktioita ja vaatii OpenGL 3.3:n.
Kaikki tarvittavat riippuvuudet olivat jo asennettuina tällä koneella.

```bash
cd ~/Code/PsyTunnel
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Myös shaderit pakataan ohjelman sisään käännöksessä. Ohjelma toimii mistä
hakemistosta tahansa, eikä tarvitse tekstuureja tai muita ladattavia resursseja.
Shaderin muokkauksen jälkeen `cmake --build build -j` päivittää ohjelman.

## Mistä liikettä muutetaan?

`tunnel_math.hpp`:n `cameraAt()` on paras aloituskohta. Sen suomenkieliset
kommentit selittävät kolme erillistä liikettä:

1. **Kameran paikka:** `radius` määrää etäisyyden keskilinjasta ja `orbit`
   kiertokulman. Sini ja kosini muuttavat nämä x- ja y-koordinaateiksi.
   Suurimmalla huojunnalla säde vaihtelee 0.06–0.80:n välillä. Putken säde
   on 1, joten kamera pysyy seinämän sisällä.
2. **Katseen kohde:** `targetAngle` kiertää epätasaisesti, välillä peruuttaen.
   `targetRadius` pitää kohteen näkymättömän ympyrän kehällä 2.7 yksikköä
   kameran edessä. Kamera ja kohde käyttävät eri aaltoja ja nopeuksia.
3. **Pieni kallistus:** `roll` keinuttaa näkymää sivusuunnassa. Kohdetta
   kohti katsova `forward` sekä `right` ja `up` muodostavat kameran akselit.

Python-vertauksena `Vec3` on pieni kolmen luvun tietue, jolle on määritelty
vektorien yhteenlasku, vähennys ja kertominen luvulla. `Camera` kokoaa ne
yhteen kuten dataclass. Lause `target-position` kertoo suunnan kamerasta
kohteeseen; `normalized()` muuttaa sen yhden yksikön pituiseksi.

`tunnel.frag` sisältää varsinaisen kuvan. Se laskee, missä jokaisesta pikselistä
lähtevä näkösäde osuu sylinterin seinään, ja värittää osuman kulman ja syvyyden
perusteella. Kaavan johtaminen, kulmasauman käsittely ja kaukaisten raitojen
suodatus on kommentoitu suomeksi. `palette()` määrittelee värit.
`paletteAt()` C++-puolella määrää niiden vaihtumisajat.

## Tarkistaminen ilman näkyvää ikkunaa

```bash
./build/psytunnel --headless --resolution 1280x720 --time 18 \
    --palette rainbow --screenshot /tmp/psytunnel.bmp
./build/psytunnel --headless --resolution 640x360 --frames 120
```

`--headless` käyttää SDL:n offscreen-ajuria. Se voi käyttää ohjelmistopohjaista
OpenGL:ää, joten sen FPS ei kerro tavallisen näyttöikkunan GPU-suorituskyvystä.
`--screenshot` tallentaa BMP-kuvan annetulla piirtoresoluutiolla. Ilman
`--frames`-valitsinta kuvakaappaus tai headless-ajo piirtää yhden ruudun.

CTest tarkistaa kameran pysymisen putkessa, katseen ympyräradan, kameran
akselien kohtisuoruuden, liikkeen jatkuvuuden, värikierron rajat ja resoluutioiden
jäsennyksen. Lisäksi demosta on tarkistettu offscreen-renderöinti ja väripaletit.
