# 3FC

**por FerdinandoPH — by FerdinandoPH**

**3DS FTP Client** — cliente FTP con interfaz gráfica para Nintendo 3DS.
**3DS FTP Client** — FTP client with a graphical interface for the Nintendo 3DS.

[Español](#español) · [English](#english)

---

## Español

### Qué es

3FC (*3DS FTP Client*) es un cliente FTP con GUI que corre en la propia consola,
escrito en C++20 con [devkitPro](https://devkitpro.org/) y [Dear ImGui](https://github.com/ocornut/imgui),
con un aspecto parecido al de [ftpd](https://github.com/mtheall/ftpd). Sirve para
mover ficheros entre la tarjeta SD de la 3DS y cualquier servidor FTP — incluida
otra 3DS corriendo ftpd, que fue la idea original.

Habla FTP plano en modo pasivo (PASV), transfiere siempre en binario y entiende
UTF-8, así que los nombres con acentos se ven y se transfieren bien.

### Características

- **5 slots de conexión** guardados en la SD, con usuario y contraseña, casilla
  de *anónimo* y un alias opcional. Para escribir se abre el teclado nativo de
  la consola.
- **Explorador doble**: tarjeta SD a un lado, servidor al otro, alternables con
  `L`/`R`. Listado por `MLSD` con tamaños; los nombres largos se recortan y se
  van deslizando cuando el cursor está encima.
- **Cola de transferencias** con barra de progreso, ETA y cancelación — la que
  está en curso o todas las pendientes de golpe. Las carpetas se transfieren
  recursivamente y, si algo ya existe, se pregunta antes de sobrescribir (con
  «sí a todo» / «no a todo»).
- **Selección múltiple** y menú de acciones: crear carpeta, renombrar, borrar,
  pegar (copia dentro de la misma máquina) y transferir entre máquinas.
- **Pestaña Consola** con el diálogo FTP crudo, comando a comando, para ver qué
  está pasando cuando un servidor se porta raro.
- **Interfaz en español e inglés**, siguiendo el idioma de la consola y
  cambiable en caliente desde el menú `START`.
- **Detalles de consola**: se bloquea el *sleep* al cerrar la tapa mientras hay
  conexión, la retroiluminación se puede apagar sin cortar las transferencias, y
  en New 3DS se activa el reloj a 804 MHz (`osSetSpeedupEnable`) para ir más
  rápido.

### Controles

Se maneja todo con botones; solo se usa el teclado táctil nativo para escribir
texto. La misma ayuda está dentro de la app, en `START` → *Controles*.

**En cualquier sitio**

| Botón | Acción |
|---|---|
| `X` | Cambia a qué pantalla afectan los botones. La pantalla activa lleva un borde azul; la otra se atenúa. |
| `START` | Abre el menú. |
| Cruceta / pad circular | Mover. Izquierda y derecha saltan una página. |

**Pantalla superior — conexión**

| Botón | Acción |
|---|---|
| `A` | Conectar, o rellenar un slot vacío. |
| `Y` | Editar el slot. |

**Pantalla superior — explorador**

| Botón | Acción |
|---|---|
| `A` | Entrar en una carpeta. |
| `B` | Subir un nivel. |
| `L` / `R` | Cambiar entre la tarjeta SD y el servidor. |
| `SELECT` | Marcar o desmarcar el elemento bajo el cursor. |
| `Y` | Acciones: crear carpeta, renombrar, borrar, pegar, transferir. |

**Pantalla inferior**

| Botón | Acción |
|---|---|
| `L` / `R` | Cambiar entre las pestañas Transferencias y Consola. |
| `A` (Transferencias) | Cancelar la transferencia en curso. |
| `Y` (Transferencias) | Cancelar todas las que están esperando. |
| Cruceta (Consola) | Desplazar el registro FTP. Deja de seguir las líneas nuevas mientras lees hacia atrás. |

La lista de transferencias muestra la actual y las terminadas; lo que sigue en
cola es el número de pendientes de arriba.

**Menú `START`**: apagar la retroiluminación (las transferencias siguen; se
enciende al pulsar cualquier botón), cambiar de idioma, desconectar, ver estos
controles, ver las licencias y salir.

### Compilar

Hace falta devkitPro con `devkitARM`, `libctru` y `citro3d`
(`dkp-pacman -S 3ds-dev`) y la variable `DEVKITARM` exportada.

```sh
make            # 3FC.3dsx + 3FC.smdh
make cia        # además 3FC.cia, para instalar en el menú HOME
make DEBUG=1    # -Og -g3, con símbolos legibles
make clean
```

`make cia` necesita además `bannertool` y `makerom` en el `PATH`.

Para instalar: copia el `.3dsx` a `sdmc:/3ds/` y lánzalo desde el Homebrew
Launcher, o instala el `.cia` con FBI.

El icono (`icon.png`) y el banner del `.cia` (`meta/banner.png`) están
versionados porque el build los necesita; se generan sin dependencias con:

```sh
python3 meta/makeart.py icon.png meta/banner.png
```

Solo hay que regenerarlos si se cambia el diseño. El audio del banner
(`meta/banner.wav`) no se genera y viaja en el repositorio.

### Configuración

Los slots viven en `sdmc:/3ds/3FC/config.cfg`, que la app crea sola la
primera vez.

> Las contraseñas de los slots se guardan **en texto plano**: la 3DS no tiene
> almacén de secretos y la SD es legible por cualquiera con acceso físico a la
> consola.

### Limitaciones

- **FTP plano, sin FTPS.** Usuario, contraseña y datos viajan sin cifrar; está
  pensado para la red local.
- **No hay copia servidor → servidor.** El protocolo no lo permite y la app no
  lo simula descargando y volviendo a subir por detrás.
- **Pendiente de verificar en hardware real**: la instalación del `.cia` en el
  menú HOME, la velocidad de transferencia por wifi, el bloqueo del *sleep* al
  cerrar la tapa y el apagado de la retroiluminación. Todo está implementado,
  pero son cosas que un emulador no puede confirmar.

### Licencia y terceros

Este proyecto es **GPL-3.0** — ver [`LICENSE`](LICENSE).

Todo lo que hay en `third_party/` es MIT y conserva su cabecera de licencia
original:

- **Dear ImGui** 1.91.8 — © Omar Cornut ([`third_party/imgui/LICENSE.txt`](third_party/imgui/LICENSE.txt)).
- **Backend de ImGui para 3DS** (`imgui_citro3d`, `imgui_ctru`, `vshader.v.pica`)
  — © 2020/2024 Michael Theall, tomado de [ftpd](https://github.com/mtheall/ftpd).
  El repositorio de ftpd es GPL-3.0 en conjunto, pero estos ficheros llevan
  licencia MIT propia. Única modificación: se eliminó un `#include "fs.h"` que
  aquí no aplica.

Ese backend construye el atlas de glifos a partir de la fuente del sistema de la
3DS, que es lo que da soporte a acentos y UTF-8 sin depender de freetype.

---

## English

### What it is

3FC (*3DS FTP Client*) is a GUI FTP client that runs on the console itself,
written in C++20 with [devkitPro](https://devkitpro.org/) and [Dear ImGui](https://github.com/ocornut/imgui),
styled after [ftpd](https://github.com/mtheall/ftpd). It moves files between the
3DS SD card and any FTP server — including another 3DS running ftpd, which was
the original idea.

It speaks plain FTP in passive mode (PASV), always transfers in binary and
handles UTF-8, so accented filenames display and transfer correctly.

### Features

- **5 connection slots** stored on the SD card, with user and password, an
  *anonymous* checkbox and an optional alias. Text entry uses the console's
  native keyboard.
- **Dual browser**: SD card on one side, server on the other, swapped with
  `L`/`R`. `MLSD` listing with sizes; long names are truncated and scroll while
  the cursor sits on them.
- **Transfer queue** with a progress bar, ETA and cancellation — the one in
  flight or every pending one at once. Folders transfer recursively, and if
  something already exists you are asked before overwriting (with "yes to all" /
  "no to all").
- **Multiple selection** and an actions menu: new folder, rename, delete, paste
  (copy within the same machine) and transfer between machines.
- **Console tab** with the raw FTP dialogue, command by command, for when a
  server misbehaves.
- **English and Spanish UI**, following the console language and switchable on
  the fly from the `START` menu.
- **Console details**: sleep is blocked while the lid is closed during a
  session, the backlight can be turned off without interrupting transfers, and
  on a New 3DS the 804 MHz clock (`osSetSpeedupEnable`) is enabled for speed.

### Controls

Everything is driven with the buttons; the native touch keyboard is only used
for text entry. The same help lives inside the app, under `START` → *Controls*.

**Anywhere**

| Button | Action |
|---|---|
| `X` | Switch which screen the buttons act on. The active screen has a blue border; the other one is dimmed. |
| `START` | Open the menu. |
| D-pad / circle pad | Move. Left and right jump a page. |

**Top screen — connection**

| Button | Action |
|---|---|
| `A` | Connect, or fill an empty slot. |
| `Y` | Edit the slot. |

**Top screen — file browser**

| Button | Action |
|---|---|
| `A` | Enter a folder. |
| `B` | Go up one level. |
| `L` / `R` | Switch between the SD card and the server. |
| `SELECT` | Mark or unmark the item under the cursor. |
| `Y` | Actions: new folder, rename, delete, paste, transfer. |

**Bottom screen**

| Button | Action |
|---|---|
| `L` / `R` | Switch between the Transfers and Console tabs. |
| `A` (Transfers) | Cancel the transfer in progress. |
| `Y` (Transfers) | Cancel every transfer still waiting. |
| D-pad (Console) | Scroll the FTP log. It stops following new lines while you are reading back. |

The transfer list shows the current transfer and the finished ones; what is
still queued is the pending count at the top.

**`START` menu**: turn off the backlight (transfers keep running; any button
turns it back on), change language, disconnect, show these controls, show the
licences, and exit.

### Building

You need devkitPro with `devkitARM`, `libctru` and `citro3d`
(`dkp-pacman -S 3ds-dev`) and `DEVKITARM` exported.

```sh
make            # 3FC.3dsx + 3FC.smdh
make cia        # also 3FC.cia, to install to the HOME menu
make DEBUG=1    # -Og -g3, with readable symbols
make clean
```

`make cia` additionally needs `bannertool` and `makerom` on the `PATH`.

To install: copy the `.3dsx` to `sdmc:/3ds/` and launch it from the Homebrew
Launcher, or install the `.cia` with FBI.

The icon (`icon.png`) and the `.cia` banner (`meta/banner.png`) are committed
because the build needs them; they are generated with no dependencies by:

```sh
python3 meta/makeart.py icon.png meta/banner.png
```

You only need to regenerate them if the design changes. The banner audio
(`meta/banner.wav`) is not generated and ships in the repository.

### Configuration

Slots live in `sdmc:/3ds/3FC/config.cfg`, which the app creates on first
run.

> Slot passwords are stored **in plain text**: the 3DS has no secret store, and
> the SD card is readable by anyone with physical access to the console.

### Limitations

- **Plain FTP, no FTPS.** Username, password and data travel unencrypted; this
  is meant for a local network.
- **No server → server copy.** The protocol does not allow it, and the app does
  not fake it by downloading and re-uploading behind your back.
- **Still to be verified on real hardware**: installing the `.cia` to the HOME
  menu, wifi transfer speed, the sleep lock when closing the lid, and turning
  the backlight off. All of it is implemented, but an emulator cannot confirm
  any of it.

### Licence and third parties

This project is **GPL-3.0** — see [`LICENSE`](LICENSE).

Everything under `third_party/` is MIT and keeps its original licence header:

- **Dear ImGui** 1.91.8 — © Omar Cornut ([`third_party/imgui/LICENSE.txt`](third_party/imgui/LICENSE.txt)).
- **3DS ImGui backend** (`imgui_citro3d`, `imgui_ctru`, `vshader.v.pica`) —
  © 2020/2024 Michael Theall, taken from [ftpd](https://github.com/mtheall/ftpd).
  The ftpd repository is GPL-3.0 as a whole, but these files carry their own MIT
  licence. Only modification: an `#include "fs.h"` that does not apply here was
  removed.

That backend builds the glyph atlas from the 3DS system font, which is what
gives accents and UTF-8 support without depending on freetype.
