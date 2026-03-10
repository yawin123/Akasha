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
- Usar `Boost.Interprocess` como base de acceso a fichero vía memoria mapeada.
- Definir políticas explícitas de crecimiento de archivos mapeados (sin realocaciones implícitas no controladas).
- Evitar copias completas del dataset mapeado hacia estructuras espejo en memoria del proceso.
- Evitar cambios de API incompatibles a partir de 0.9.0.

## Estado actual (0.3.0 completado)

- Librería estática compilando con CMake.
- Dependencias integradas con Conan (Boost + FlatBuffers).
- API core implementada con `Store`, `DatasetView`, `load`, `has`, `get` y notación por puntos.
- Ejemplo funcional consultando valores y subárboles.
- Carga desde fichero FlexBuffers implementada con `Boost.Interprocess` (`file_mapping` + `mapped_region`) en modo lectura.
- Resolución de consultas sobre referencias a memoria mapeada (sin materializar valores hoja en un contenedor separado).
- Soporte completo de escalares: bool, int64, uint64, double, string (zero-copy para strings).
- Próximo foco: 0.4.0 (fusión de múltiples fuentes) o 0.5.0 (persistencia con redimensionado dinámico).

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

**Meta:** leer y consultar datos reales desde archivos FlexBuffers mapeados, minimizando copia y latencia.

**Entregables:**

- Loader de fichero basado en `Boost.Interprocess` con `file_mapping` + `mapped_region`.
- Estructura de apoyo permitida solo para navegación (índices/rutas), sin duplicar los valores hoja fuera del mapeo.
- Validaciones de formato y manejo de errores de parseo.
- Soporte de tipos básicos: bool, int, uint, double, string.
- Tests con fixtures FlexBuffers pequeñas.

**Done cuando:**

- Se pueden cargar N ficheros independientes y consultar valores por clave.
- Los errores de parseo y claves no encontradas son distinguibles.
- Las lecturas de valores se resuelven desde memoria mapeada (zero-copy para strings y sin volcado completo a `DatasetSource`).

**Estado:** completado.

**Completado:**

- Lectura de FlexBuffers desde archivo mapeado (`mmap`) en modo `read_only`.
- Parseo de mapa raíz y resolución por ruta con referencias al `mapped_region`.
- Distinción de errores de lectura, parseo y tipo no soportado.
- Escalares completos: bool, int64, uint64, double, string.
- Consultas devuelven `std::nullopt` cuando la clave no existe.
- Zero-copy: strings expuestos como `string_view` sobre memoria mapeada.

**Notas de alcance:**

- Por decisión de alcance, tests y fixtures se difieren a hitos posteriores de endurecimiento.

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
- Persistencia sobre archivo mapeado con política de crecimiento.
- Redimensionado dinámico del fichero mapeado cuando la capacidad sea insuficiente (grow + remap seguro).
- Exportación a FlexBuffers (o formato intermedio documentado).
- Tests de round-trip (load -> set -> save -> load).

**Done cuando:**

- Se demuestra round-trip sin pérdida en tipos soportados.
- El crecimiento de fichero mapeado es transparente para el consumidor y mantiene consistencia.

## 0.6.0 — Base interproceso (MVP)

**Meta:** preparar uso compartido entre procesos.

**Entregables:**

- Abstracción de backend de almacenamiento (in-memory vs shared-memory).
- Prototipo funcional con Boost.Interprocess para lectura compartida.
- Coordinación de remapeo entre procesos (estrategia de sincronización y visibilidad tras resize).
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

1. Cerrar 0.3.0 (fixtures + validación fina de errores en carga FlexBuffers por mapeo).
2. Implementar 0.4.0 (merge + precedencia).
3. Diseñar e implementar 0.5.0 con redimensionado dinámico seguro de archivos mapeados.

Con esos tres hitos, Akasha ya tendría una propuesta de valor usable y medible.
