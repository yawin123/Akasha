# Akasha

Akasha es una librería C++23 orientada a la gestión de datos configurables y estructurados, diseñada para cargar múltiples fuentes de datos y exponer su acceso de forma uniforme.

El objetivo principal es que los consumidores de la librería trabajen con una interfaz simple basada en tres conceptos:

- **Set de datos**: viene implícito en el primer segmento de la clave.
- **Clave**: identificador jerárquico con formato mínimo `dataset.algo`.
- **Valor**: contenido asociado a la clave.

## Visión del proyecto

Akasha actúa como una capa de abstracción sobre distintos ficheros de datos. Conceptualmente, puede pensarse como abrir varios documentos tipo JSON y consultar sus claves, pero en este proyecto se empleará:

- **Boost.Interprocess managed_mapped_file** como backend de almacenamiento persistente.
- **Boost.Interprocess** para soportar estrategias de compartición/interacción entre procesos.

La librería permite cargar *N* ficheros de datos y resolver lecturas sin que el usuario final tenga que preocuparse por los detalles internos de almacenamiento.

## Modelo de acceso

El acceso usa claves completas en dot notation y **siempre calificadas por dataset**.

### Reglas de clave

- Formato mínimo: `dataset.algo`.
- El primer segmento identifica el dataset cargado con `load(source_id, ...)`.
- No hay búsqueda cross-dataset: si el dataset no existe, `get(...)` devuelve `nullopt`.

Ejemplos:

- `user.core.timeout`
- `defaults.core.settings.enabled`
- `user.service.host`

### API actual de carga y consulta

- `load(source_id, file_path, create_if_missing = false)`
	- Abre/crea un archivo mapeado y asocia `source_id` como dataset lógico.
	- Si `source_id` ya está cargado, devuelve `source_already_loaded`.
	- Si el archivo no existe:
		- `create_if_missing = false` → `file_read_error`
		- `create_if_missing = true` → crea dataset vacío.
- `set(key_path, value)`
	- Escribe directamente sobre el archivo mapeado y hace `flush` inmediato.
- `get(key_path)` devuelve:
	- `std::nullopt` si no existe,
	- `ValueView` si apunta a hoja,
	- `DatasetView` si apunta a nodo intermedio (por ejemplo `user.core.settings`).

Este enfoque evita ambigüedad entre fuentes y garantiza resolución determinista por dataset.

## ¿Por qué el nombre Akasha?

El nombre **Akasha** proviene del sánscrito **ākāśa**, término que suele traducirse como “éter”, “espacio” o “cielo”. En tradiciones filosóficas de la India, *ākāśa* se entiende como el medio sutil que contiene o posibilita la existencia y transmisión de las cosas.

Con el tiempo, ese concepto también se popularizó (especialmente en corrientes esotéricas) como la idea de un “registro” o repositorio universal de información. Aunque el proyecto no adopta ese marco espiritual, sí toma la metáfora técnica: una capa común donde datos de múltiples orígenes se agregan, se ordenan y se consultan con una interfaz uniforme.

Por eso **Akasha** encaja con la librería: porque expresa la noción de “espacio unificado de datos”, independientemente del fichero o backend del que provenga cada valor.

## Estado actual

El repositorio está configurado como librería **estática** en **C++23**. Este enfoque facilita encapsular dependencias internas y ofrecer una integración más predecible a los consumidores.

Estructura base actual:

```text
.
├── CMakeLists.txt
├── cmake/BundleStaticArchive.cmake
├── include/akasha.hpp
├── src/akasha.cpp
└── apps/example.cpp
```

## Dependencias y empaquetado

Akasha usa **Conan** para gestionar sus dependencias:

- **Boost 1.90.0** (especialmente `Boost.Interprocess`)

Las dependencias se especifican en `conanfile.py` y se instalan automáticamente via Conan.

En CMake, Akasha expone dos modos:

- **Modo normal (recomendado por defecto)**: construye `libakasha.a` y enlaza dependencias estáticas en el binario final consumidor.
- **Modo single-archive (opcional)**: genera `libakasha_bundle.a`, intentando agrupar Akasha y sus archivos estáticos dependientes en un único artefacto.

Opciones disponibles:

- `AKASHA_DEP_TARGETS`: lista separada por `;` con targets estáticos CMake a enlazar como dependencias privadas.
- `AKASHA_BUNDLE_ARCHIVES`: lista separada por `;` con rutas adicionales a `.a` para incluir en el bundle.
- `AKASHA_BUILD_SINGLE_ARCHIVE`: `ON/OFF` para activar la creación de `libakasha_bundle.a`.

## Compilación

Primero, instala las dependencias con Conan:

```bash
conan install . --output-folder=build --build=missing
```

Build normal:

```bash
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
```

Build con bundle (archivo único):

```bash
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
	-DAKASHA_DEP_TARGETS="dep1;dep2" \
	-DAKASHA_BUILD_SINGLE_ARCHIVE=ON
cmake --build . --target akasha_bundle
```

> Nota: el modo `single-archive` usa `ar/ranlib` y está orientado a toolchains tipo GNU/Unix.

