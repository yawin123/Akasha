# Akasha

Akasha es una librería C++23 orientada a la gestión de datos configurables y estructurados, diseñada para cargar múltiples fuentes de datos y exponer su acceso de forma uniforme.

El objetivo principal es que los consumidores de la librería trabajen con una interfaz simple basada en tres conceptos:

- **Set de datos**: conjunto lógico que agrupa información cargada desde un fichero.
- **Clave**: identificador del dato dentro de un set.
- **Valor**: contenido asociado a la clave.

## Visión del proyecto

Akasha actúa como una capa de abstracción sobre distintos ficheros de datos. Conceptualmente, puede pensarse como abrir varios documentos tipo JSON y consultar sus claves, pero en este proyecto se empleará:

- **FlexData** como formato/base de datos de entrada.
- **bootstrap::interprocess** para soportar estrategias de compartición/interacción entre procesos.

La librería debe permitir cargar *N* ficheros de datos y resolver lecturas/escrituras sin que el usuario final tenga que preocuparse por los detalles internos de almacenamiento o transporte.

## Modelo de acceso

El acceso está diseñado para ser transparente y consistente, utilizando la semántica:

`set de datos` + `clave` + `valor`

Además, las claves soportan notación jerárquica por segmentos (dot notation), por ejemplo:

- `core.timeout` → clave `timeout` dentro del grupo `core`.

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

La estrategia recomendada es gestionar dependencias con **Git submodules** y enlazarlas de forma `PRIVATE` dentro de Akasha.

En CMake, Akasha expone dos modos:

- **Modo normal (recomendado por defecto)**: construye `libakasha.a` y enlaza dependencias estáticas en el binario final consumidor.
- **Modo single-archive (opcional)**: genera `libakasha_bundle.a`, intentando agrupar Akasha y sus archivos estáticos dependientes en un único artefacto.

Opciones disponibles:

- `AKASHA_DEP_TARGETS`: lista separada por `;` con targets estáticos CMake a enlazar como dependencias privadas.
- `AKASHA_BUNDLE_ARCHIVES`: lista separada por `;` con rutas adicionales a `.a` para incluir en el bundle.
- `AKASHA_BUILD_SINGLE_ARCHIVE`: `ON/OFF` para activar la creación de `libakasha_bundle.a`.

## Compilación

Build normal:

```bash
cmake -S . -B build
cmake --build build
```

Build con bundle (archivo único):

```bash
cmake -S . -B build \
	-DAKASHA_DEP_TARGETS="dep1;dep2" \
	-DAKASHA_BUILD_SINGLE_ARCHIVE=ON
cmake --build build --target akasha_bundle
```

> Nota: el modo `single-archive` usa `ar/ranlib` y está orientado a toolchains tipo GNU/Unix.

