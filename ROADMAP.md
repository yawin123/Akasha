# ROADMAP Akasha hacia 1.0.0

## Objetivo de 1.0.0

Entregar una librería C++23 estable para cargar múltiples fuentes de configuración en FlexBuffers y exponer una API uniforme basada en:

- set de datos
- clave (con dot notation)
- valor

Con soporte de uso embebido (misma aplicación) y una base sólida para escenarios interproceso con Boost.Interprocess.

## Principios de evolución

- Mantener API pública pequeña y clara.
- Priorizar estabilidad y testabilidad sobre features “nice to have”.
- Asegurar builds reproducibles con Conan + CMake.
- Evitar cambios de API incompatibles a partir de 0.9.0.

## Estado actual (0.2.0 completado)

- Librería estática compilando con CMake.
- Dependencias integradas con Conan (Boost + FlatBuffers).
- API core implementada con `Store`, `DatasetView`, `load`, `has`, `get` y notación por puntos.
- Ejemplo funcional consultando valores y subárboles.
- Próximo foco: 0.3.0 (carga real desde FlexBuffers).

## Hitos por versión

## 0.2.0 — Núcleo de dominio y API mínima

**Meta:** sustituir placeholders por el modelo real de Akasha.

**Entregables:**

- Tipos base: `KeyPath`, `Value`, `ValueView`, `DatasetSource`.
- API mínima:
  - `load(source)`
  - `has(key_path)`
  - `get(key_path)`
- Navegación jerárquica vía `DatasetView` cuando `get` apunta a nodo intermedio.
- Resolución de claves con dot notation (`core.timeout`).
- Primer conjunto de tests unitarios del core.

**Done cuando:**

- El ejemplo usa la API real (sin `add()`).
- Cobertura básica de paths válidos/invalidos y conflictos de clave.

**Estado:** completado.

**Completado:**

- API real del core implementada y documentada.
- Ejemplo actualizado y compilando.
- Dot notation y acceso jerárquico funcionando.

**Notas de alcance:**

- Por decisión de alcance, los tests unitarios del core se difieren a un hito posterior.
- `version()` actualizada a `0.2.0`.

## 0.3.0 — Carga real desde FlexBuffers

**Meta:** leer datos reales desde archivos FlexBuffers.

**Entregables:**

- Loader de fichero a estructura interna in-memory.
- Validaciones de formato y manejo de errores de parseo.
- Soporte de tipos básicos: bool, int, double, string.
- Tests con fixtures FlexBuffers pequeñas.

**Done cuando:**

- Se pueden cargar N ficheros independientes y consultar valores por clave.
- Los errores de parseo y claves no encontradas son distinguibles.

## 0.4.0 — Fusión de múltiples fuentes

**Meta:** resolver conflictos y precedencia entre datasets.

**Entregables:**

- Política de merge configurable (por defecto: último gana).
- Consulta agregada sobre varios datasets.
- Definición de orden de prioridad por dataset.
- Tests de conflicto y precedencia.

**Done cuando:**

- El usuario puede consultar una “vista unificada” de varias fuentes.

## 0.5.0 — Escritura controlada y persistencia

**Meta:** permitir actualizaciones seguras de valores.

**Entregables:**

- API de actualización: `set(id, key, value)`.
- Reglas de tipado y validación en escritura.
- Exportación a FlexBuffers (o formato intermedio documentado).
- Tests de round-trip (load -> set -> save -> load).

**Done cuando:**

- Se demuestra round-trip sin pérdida en tipos soportados.

## 0.6.0 — Base interproceso (MVP)

**Meta:** preparar uso compartido entre procesos.

**Entregables:**

- Abstracción de backend de almacenamiento (in-memory vs shared-memory).
- Prototipo funcional con Boost.Interprocess para lectura compartida.
- Reglas de ownership y ciclo de vida documentadas.
- Tests básicos de lectura desde proceso secundario.

**Done cuando:**

- Existe al menos un escenario reproducible de lectura interproceso.

## 0.7.0 — Modelo de errores y observabilidad

**Meta:** mejorar diagnósticos y soporte de operación.

**Entregables:**

- Jerarquía clara de errores/códigos (`parse_error`, `not_found`, etc.).
- Mensajes de error consistentes para consumidor.
- Hooks de logging trazables (sin acoplar a framework concreto).
- Tests de errores y contratos.

**Done cuando:**

- Los fallos más frecuentes están cubiertos con mensajes accionables.

## 0.8.0 — Empaquetado y DX de integración

**Meta:** facilitar adopción por terceros.

**Entregables:**

- Instalación CMake (`install`, `export`, `find_package`) validada.
- Ejemplo consumidor externo (fuera del repo) documentado.
- Ajustes finales de Conan recipe para consumo limpio.
- Guía de integración en README.

**Done cuando:**

- Un proyecto externo puede consumir Akasha sin hacks locales.

## 0.9.0 — Beta de estabilización

**Meta:** congelar superficie pública de cara a 1.0.

**Entregables:**

- Revisión de API pública y eliminación de deuda crítica.
- Compatibilidad de API: sin breaking changes no justificados.
- Endurecimiento de tests (unit + integración).
- Benchmarks básicos de latencia de consulta.

**Done cuando:**

- API candidata a estable y feedback interno cerrado.

## 1.0.0 — Release estable

**Meta:** primera versión estable orientada a producción.

**Entregables:**

- API estable documentada.
- Matriz mínima de compatibilidad (compilador/SO) publicada.
- Changelog y guía de migración desde 0.x.
- Tag/release reproducible en CI.

**Criterios de salida 1.0.0:**

- Sin issues críticos abiertos en core API, parseo y merge.
- Contratos de errores y comportamiento documentados.
- Build e integración verificables desde cero con Conan + CMake.

## Fuera de alcance (antes de 1.0.0)

- Hot reload avanzado.
- Sistema de plugins de backends.
- Replicación distribuida entre nodos.
- Bindings para otros lenguajes.

## Orden recomendado de ejecución inmediata

1. Implementar 0.3.0 (loader FlexBuffers + fixtures).
2. Implementar 0.4.0 (merge + precedencia).
3. Reintroducir tests unitarios del core como parte del endurecimiento de calidad.

Con esos tres hitos, Akasha ya tendría una propuesta de valor usable y medible.
