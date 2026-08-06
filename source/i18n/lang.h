#pragma once

/// Every translatable string in the app. Add entries here and then to both
/// tables in lang.cpp; the build breaks if a table gets out of sync.
#define I18N_STRINGS(X)                                                                            \
	X (APP_TITLE, "3FC", "3FC")                                                                    \
	/* generic */                                                                                  \
	X (YES, "Yes", "Sí")                                                                           \
	X (NO, "No", "No")                                                                             \
	X (YES_ALL, "Yes to all", "Sí a todo")                                                         \
	X (NO_ALL, "No to all", "No a todo")                                                           \
	X (CANCEL, "Cancel", "Cancelar")                                                               \
	X (OK, "OK", "Aceptar")                                                                        \
	X (BACK, "Back", "Atrás")                                                                      \
	/* start menu */                                                                               \
	X (MENU, "Menu", "Menú")                                                                       \
	X (MENU_BACKLIGHT, "Turn off backlight", "Apagar retroiluminación")                            \
	X (MENU_BACKLIGHT_HINT,                                                                        \
	    "Transfers keep running. Press any button to turn it back on.",                            \
	    "Las transferencias siguen. Pulsa cualquier botón para encenderla.")                       \
	X (MENU_LANGUAGE, "Language: English", "Idioma: Español")                                      \
	X (MENU_DISCONNECT, "Disconnect", "Desconectar")                                               \
	X (MENU_HELP, "Controls", "Controles")                                                         \
	X (MENU_LICENSES, "Licences", "Licencias")                                                     \
	X (MENU_EXIT, "Exit", "Salir")                                                                 \
	X (MENU_CLOSE, "Close menu", "Cerrar menú")                                                    \
	X (EXIT_CONFIRM, "Exit the application?", "¿Salir de la aplicación?")                          \
	X (EXIT_CONFIRM_TRANSFERS,                                                                     \
	    "There are transfers in progress. Exit and cancel them?",                                  \
	    "Hay transferencias en curso. ¿Salir y cancelarlas?")                                      \
	/* bottom screen */                                                                            \
	X (TAB_PROGRESS, "Transfers", "Transferencias")                                                \
	X (TAB_CONSOLE, "Console", "Consola")                                                          \
	X (NO_TRANSFERS, "No transfers.", "No hay transferencias.")                                    \
	X (EXPANDING, "Scanning folders...", "Explorando carpetas...")                                 \
	X (PENDING_COUNT, "Pending (%d)", "Pendientes (%d)")                                           \
	X (PENDING_COUNT_MIN, "Pending (%d or more)", "Pendientes (%d o más)")                         \
	X (STATUS_PENDING, "pending", "pendiente")                                                     \
	X (STATUS_DONE, "done", "hecho")                                                               \
	X (STATUS_CANCELLED, "cancelled", "cancelada")                                                 \
	X (STATUS_FAILED, "failed", "error")                                                           \
	X (CANCEL_ONE, "Cancel this transfer?", "¿Cancelar esta transferencia?")                       \
	X (CANCEL_PENDING, "Cancel all pending transfers?", "¿Cancelar todas las pendientes?")         \
	/* status bar */                                                                               \
	X (OFFLINE, "offline", "sin conexión")                                                         \
	/* connection screen */                                                                        \
	X (CONNECT_LAST, "Connect to last used", "Conectar a la última usada")                         \
	X (CONNECT_SAVED, "Saved connections", "Conexiones guardadas")                                 \
	X (SLOT_EMPTY, "<empty>", "<vacío>")                                                           \
	X (SLOT_HINT_EMPTY, "A: fill this slot", "A: rellenar este slot")                              \
	X (SLOT_HINT_FILLED, "A: connect    Y: edit", "A: conectar    Y: editar")                      \
	X (FIELD_HOST, "Address", "Dirección")                                                         \
	X (FIELD_PORT, "Port", "Puerto")                                                               \
	X (FIELD_USER, "User", "Usuario")                                                              \
	X (FIELD_PASSWORD, "Password", "Contraseña")                                                   \
	X (FIELD_ANONYMOUS, "Anonymous", "Anónimo")                                                    \
	X (FIELD_ALIAS, "Alias (optional)", "Alias (opcional)")                                        \
	X (SLOT_SAVE, "Save", "Guardar")                                                               \
	X (SLOT_DELETE, "Delete slot", "Borrar slot")                                                  \
	X (SLOT_DELETE_CONFIRM, "Delete this saved connection?", "¿Borrar esta conexión guardada?")    \
	X (SLOT_NEEDS_HOST, "An address is required.", "Hace falta una dirección.")                    \
	X (PASSWORD_PLAINTEXT,                                                                         \
	    "Saved in plain text on the SD card.",                                                     \
	    "Se guarda en texto plano en la tarjeta SD.")                                              \
	X (CONNECTING, "Connecting to %s...", "Conectando a %s...")                                    \
	X (CONNECT_FAILED, "Could not connect.", "No se pudo conectar.")                               \
	X (CONNECTION_LOST,                                                                            \
	    "The connection to the server was lost. Transfers in progress were cancelled.",            \
	    "Se perdió la conexión con el servidor. Se cancelaron las transferencias en curso.")       \
	X (CONNECT_ABORT_HINT, "B: cancel", "B: cancelar")                                           \
	X (NO_WORKER,                                                                                  \
	    "The network thread could not start. Transfers are unavailable.",                          \
	    "No se pudo iniciar el hilo de red. Las transferencias no funcionarán.")                               \
	/* browser */                                                                                  \
	X (MACHINE_LOCAL, "SD card", "Tarjeta SD")                                                     \
	X (MACHINE_REMOTE, "Server", "Servidor")                                                       \
	X (LOADING, "Loading...", "Cargando...")                                                       \
	X (EMPTY_FOLDER, "Empty folder", "Carpeta vacía")                                              \
	X (DISCONNECT_CONFIRM, "Disconnect from the server?", "¿Desconectar del servidor?")            \
	X (SELECTION_LOST,                                                                             \
	    "This will clear the previous selection. Continue?",                                       \
	    "Se perderá la selección anterior. ¿Continuar?")                                           \
	/* actions menu */                                                                             \
	X (ACTIONS, "Actions", "Acciones")                                                             \
	X (ACTION_NEW_FOLDER, "New folder", "Crear carpeta")                                           \
	X (ACTION_RENAME, "Rename \"%s\"", "Renombrar \"%s\"")                                         \
	X (ACTION_DELETE, "Delete \"%s\"", "Borrar \"%s\"")                                            \
	X (ACTION_DELETE_SELECTED, "Delete selected (%d)", "Borrar seleccionados (%d)")                \
	X (ACTION_PASTE, "Paste selected (%d)", "Pegar seleccionados (%d)")                            \
	X (ACTION_TRANSFER, "Transfer selected (%d)", "Transferir seleccionados (%d)")                 \
	X (ACTION_CLOSE, "Close", "Cerrar")                                                            \
	X (PROMPT_NEW_FOLDER, "Folder name", "Nombre de la carpeta")                                   \
	X (PROMPT_RENAME, "New name", "Nombre nuevo")                                                  \
	X (CONFIRM_DELETE, "Delete \"%s\"? Folders are deleted with their contents.",                   \
	    "¿Borrar \"%s\"? Las carpetas se borran con su contenido.")                                 \
	X (CONFIRM_DELETE_SELECTED,                                                                    \
	    "Delete the %d selected items? Folders are deleted with their contents.",                  \
	    "¿Borrar los %d elementos seleccionados? Las carpetas se borran con su contenido.")        \
	X (CONFIRM_OVERWRITE,                                                                          \
	    "\"%s\" already exists here. Overwrite?",                                                   \
	    "\"%s\" ya existe aquí. ¿Sobrescribir?")                                                    \
	X (PASTE_REMOTE_UNSUPPORTED,                                                                   \
	    "FTP cannot copy between server folders.",                                                 \
	    "FTP no permite copiar entre carpetas del servidor.")                                      \
	/* controls help. One hint line replaces the per-screen ones, which had run  */                \
	/* out of room long before they had run out of buttons to explain.           */                \
	X (HELP_HINT, "Controls: START menu", "Controles: menú START")                                 \
	X (HELP_TITLE, "Controls", "Controles")                                                        \
	X (HELP_BOTH_TITLE, "Anywhere", "En cualquier sitio")                                          \
	X (HELP_BOTH,                                                                                  \
	    "X: switch which screen the buttons act on. The active screen has a "                      \
	    "blue border; the other one is dimmed.\n"                                                  \
	    "START: this menu.\n"                                                                      \
	    "D-pad / circle pad: move. Left and right jump a page.",                                   \
	    "X: cambia a qué pantalla afectan los botones. La pantalla activa lleva "                  \
	    "un borde azul; la otra se atenúa.\n"                                                      \
	    "START: este menú.\n"                                                                      \
	    "Cruceta / pad circular: mover. Izquierda y derecha saltan una página.")                   \
	X (HELP_TOP_TITLE, "Top screen", "Pantalla superior")                                          \
	X (HELP_TOP,                                                                                   \
	    "Connection screen\n"                                                                      \
	    "A: connect, or fill an empty slot.   Y: edit the slot.\n"                                 \
	    "\n"                                                                                       \
	    "File browser\n"                                                                           \
	    "A: enter a folder.   B: go up one level.\n"                                               \
	    "L / R: switch between the SD card and the server.\n"                                      \
	    "SELECT: mark or unmark the item under the cursor.\n"                                      \
	    "Y: actions (new folder, rename, delete, paste, transfer).",                               \
	    "Pantalla de conexión\n"                                                                   \
	    "A: conectar, o rellenar un slot vacío.   Y: editar el slot.\n"                            \
	    "\n"                                                                                       \
	    "Explorador\n"                                                                             \
	    "A: entrar en una carpeta.   B: subir un nivel.\n"                                         \
	    "L / R: cambiar entre la tarjeta SD y el servidor.\n"                                      \
	    "SELECT: marcar o desmarcar el elemento bajo el cursor.\n"                                 \
	    "Y: acciones (crear carpeta, renombrar, borrar, pegar, transferir).")                      \
	X (HELP_BOTTOM_TITLE, "Bottom screen", "Pantalla inferior")                                    \
	X (HELP_BOTTOM,                                                                                \
	    "L / R: switch between the Transfers and Console tabs.\n"                                  \
	    "\n"                                                                                       \
	    "Transfers\n"                                                                              \
	    "A: cancel the transfer in progress.\n"                                                    \
	    "Y: cancel every transfer still waiting.\n"                                                \
	    "The list shows the current transfer and the finished ones; what is "                      \
	    "still queued is the pending count at the top.\n"                                          \
	    "\n"                                                                                       \
	    "Console\n"                                                                                \
	    "D-pad / circle pad: scroll the FTP log. It stops following new lines "                    \
	    "while you are reading back.",                                                             \
	    "L / R: cambiar entre las pestañas Transferencias y Consola.\n"                            \
	    "\n"                                                                                       \
	    "Transferencias\n"                                                                         \
	    "A: cancelar la transferencia en curso.\n"                                                 \
	    "Y: cancelar todas las que están esperando.\n"                                             \
	    "La lista muestra la transferencia actual y las terminadas; lo que "                       \
	    "sigue en cola es el número de pendientes de arriba.\n"                                    \
	    "\n"                                                                                       \
	    "Consola\n"                                                                                \
	    "Cruceta / pad circular: desplazar el registro FTP. Deja de seguir las "                   \
	    "líneas nuevas mientras lees hacia atrás.")

namespace i18n
{
enum Language
{
	ENGLISH,
	SPANISH,
};

enum Str
{
#define I18N_ENUM(name, en, es) name,
	I18N_STRINGS (I18N_ENUM)
#undef I18N_ENUM
	    STR_COUNT,
};

/// \brief Pick the initial language from the console's system language.
void init ();

void setLanguage (Language lang);
Language language ();

char const *tr (Str str);
}

#define TR(name) ::i18n::tr (::i18n::name)
