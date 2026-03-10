# Akasha

Akasha es una librería C++23 orientada a la gestión de datos configurables y estructurados, diseñada para cargar múltiples fuentes de datos y exponer su acceso de forma uniforme.

El objetivo principal es que los consumidores de la librería trabajen con una interfaz simple basada en tres conceptos:

- **Set de datos**: viene implícito en el primer segmento de la clave.
- **Clave**: identificador jerárquico con formato mínimo `dataset.algo`.
- **Valor**: contenido asociado a la clave.

## Visión del proyecto

Akasha actúa como una capa de abstracción sobre distintos ficheros de datos. Conceptualmente, puede pensarse como abrir varios documentos tipo JSON y consultar sus claves, pero en este proyecto se empleará:

- **FlexData** como formato/base de datos de entrada.
- **Boost.Interprocess** para soportar estrategias de compartición/interacción entre procesos.

La librería debe permitir cargar *N* ficheros de datos y resolver lecturas/escrituras sin que el usuario final tenga que preocuparse por los detalles internos de almacenamiento o transporte.

## Modelo de acceso

El acceso está diseñado para ser transparente y consistente usando claves completas en dot notation.

Las claves soportan notación jerárquica por segmentos (dot notation), por ejemplo:

- `core.timeout` → dataset `core`, clave `timeout`.
- `core.settings.timeout` → dataset `core`, clave jerárquica `settings.timeout`.

Las claves válidas deben tener al menos dos segmentos (`algo.algo`), para evitar valores sin dataset.

En la API, `get(path)` devuelve un `optional` que puede contener:

- un **valor** (si `path` apunta a hoja), o
- un **subconjunto/dataset view** (si `path` apunta a un nodo intermedio, como `core` o `core.settings`).

Este enfoque simplifica la organización de datos complejos y mantiene una API intuitiva para los usuarios de la librería.

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
- **FlatBuffers 25.9.23** (FlexBuffers)

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

