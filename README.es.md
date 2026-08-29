# 3FC

**por FerdinandoPH** — [English](README.md) · Español

**3FC** (*3DS FTP Client*) es un cliente FTP con interfaz gráfica que corre en la
propia Nintendo 3DS. Su uso principal es mover ficheros **directamente entre dos
3DS** — una con 3FC y la otra con [ftpd](https://github.com/mtheall/ftpd) — sin PC de
por medio, aunque habla igual de bien con cualquier servidor FTP.

Está escrito en C++20 con [devkitPro](https://devkitpro.org/) y
[Dear ImGui](https://github.com/ocornut/imgui), usa FTP plano en modo pasivo (PASV),
transfiere siempre en binario y entiende UTF-8, así que los nombres con acentos se ven
y se transfieren bien.

## Características

- **5 slots de conexión** guardados en la SD: usuario, contraseña, casilla de
  *anónimo* y un alias opcional, escritos con el teclado nativo de la consola.
- **Explorador doble**: tarjeta SD a un lado, servidor al otro, alternables con `L`/`R`.
- **Cola de transferencias** con barra de progreso, ETA y cancelación; las carpetas se
  transfieren recursivamente y se pregunta antes de sobrescribir.
- **Selección múltiple** y menú de acciones: crear carpeta, renombrar, borrar, pegar y
  transferir entre máquinas.
- **Pestaña Consola** con el diálogo FTP crudo, comando a comando.
- **Interfaz en español e inglés**, siguiendo el idioma de la consola y cambiable en
  caliente.
- **Detalles de consola**: se bloquea el *sleep* mientras hay sesión, la
  retroiluminación se puede apagar sin cortar las transferencias y en New 3DS se
  trabaja a 804 MHz.

## Controles

Se maneja todo con botones; el teclado táctil solo se usa para escribir texto. La misma
ayuda está dentro de la app, en `START` → *Controles*.

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

El menú `START` apaga la retroiluminación (las transferencias siguen; se enciende al
pulsar cualquier botón), cambia el idioma, desconecta, muestra estos controles, muestra
las licencias y sale.

## Instalación

Copia `3FC.3dsx` a `sdmc:/3ds/` y lánzalo desde el Homebrew Launcher, o instala
`3FC.cia` con FBI para tenerlo en el menú HOME.

Los slots de conexión se guardan en `sdmc:/3ds/3FC/config.cfg`, que la app crea la
primera vez.

> Las contraseñas de los slots se guardan **en texto plano**: la 3DS no tiene almacén
> de secretos y la SD es legible por cualquiera con acceso físico a la consola.

## Desarrollo

Hace falta devkitPro con `devkitARM`, `libctru` y `citro3d` (`dkp-pacman -S 3ds-dev`)
y la variable `DEVKITARM` exportada. `make cia` necesita además `bannertool` y
`makerom` en el `PATH`.

```sh
make            # 3FC.3dsx + 3FC.smdh
make cia        # además 3FC.cia
make DEBUG=1    # -Og -g3, con símbolos legibles
make clean
```

Al publicar una release en GitHub se ejecuta `.github/workflows/release.yml`, que compila
en el contenedor de devkitPro y adjunta `3FC.3dsx`, `3FC.smdh` y `3FC.cia` a la release.

Para probar una compilación en hardware, abre el Homebrew Launcher en la consola y
ejecuta `3dslink 3FC.3dsx`; un emulador como Azahar también carga el `.3dsx`
directamente.

Estructura: `source/` es la aplicación — `net/` (sockets no bloqueantes), `ftp/`
(protocolo y sesión), `fs/` (operaciones de ficheros locales y remotas), `transfer/`
(la cola), `ui/` (pantallas y paneles de ImGui), `i18n/` y `config/`. En
`third_party/` viven Dear ImGui y su backend para 3DS, y en `meta/` el banner y el RSF
del CIA.

El icono y la imagen del banner se regeneran con
`python3 meta/makeart.py icon.png meta/banner.png`.

## Licencia

3FC es **GPL-3.0** — ver [`LICENSE`](LICENSE).

Todo lo que hay en `third_party/` es MIT y conserva su cabecera de licencia original:
Dear ImGui 1.91.8 © Omar Cornut ([`third_party/imgui/LICENSE.txt`](third_party/imgui/LICENSE.txt))
y el backend de ImGui para 3DS (`imgui_citro3d`, `imgui_ctru`, `vshader.v.pica`)
© 2020/2024 Michael Theall, tomado de [ftpd](https://github.com/mtheall/ftpd).
