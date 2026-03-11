# ROADMAP Akasha hacia 1.0.0

## Objetivo de 1.0.0

Entregar una librería C++23 estable para cargar múltiples fuentes de configuración en archivos mapeados persistentes y exponer una API uniforme basada en:

- set de datos
- clave (con dot notation)
- valor

Con soporte de uso embebido (misma aplicación), priorizando latencia baja y persistencia directa para almacenamiento rápido.

## Principios de evolución

- Mantener API pública pequeña y clara.
- Priorizar estabilidad y testabilidad sobre features “nice to have”.
- Asegurar builds reproducibles con Conan + CMake.
- Usar `Boost.Interprocess` como base de acceso a fichero vía memoria mapeada.
- Definir políticas explícitas de crecimiento de archivos mapeados (sin realocaciones implícitas no controladas).
- Evitar copias completas del dataset mapeado hacia estructuras espejo en memoria del proceso.
- Evitar cambios de API incompatibles a partir de 0.9.0.

## Estado actual (0.6.0)

- Librería estática compilando con CMake.
- Versión obtenida desde CMake (`AKASHA_VERSION` macro configurada en compilación).
- Dependencias integradas con Conan (Boost).
- API core implementada con `Store`, `DatasetView`, `load`, `has`, `get` y notación por puntos.
- Backend de almacenamiento persistente con `Boost.Interprocess::managed_mapped_file`.
- Cada dataset se identifica por `source_id`; duplicados rechazados con `source_already_loaded`.
- Claves siempre calificadas por dataset (`dataset.algo...`).
- `get`/`has` no hacen búsqueda cross-dataset: si el dataset no existe, devuelven `std::nullopt`/`false`.
- Flag `create_if_missing` opcional: si true, crea archivo vacío (mapa raíz vacío) si no existe.
- Soporte completo de escalares: bool, int64, uint64, double, string.
- Escritura directa con `set(key_path, value)` y `flush` inmediato.
- Política de crecimiento dinámico del archivo mapeado (`grow + remap` seguro).
- API de limpieza `clear(key_path={})` con borrado total/dataset/prefijo.
- Reset determinista al tamaño inicial del archivo en limpiezas completas.
- Concurrencia local por hilo con `std::shared_mutex` por fichero.
- **Tuning configurable**: `set_performance_tuning()` para inicial_size, grow_step, max_retries.
- **Compactación explícita**: `compact(dataset_id={})` para optimizar archivos bajo demanda.
- **Helpers template ergonómicos**: `get<T>(clave)` y `set<T>(clave, valor)` para acceso type-safe.
- Ejemplo funcional validando round-trip, multihilo y operaciones de 0.6.0.
- Foco activo: 0.7.0 (modelo de errores y observabilidad).

## Hitos por versión

## 0.2.0 — Núcleo de dominio y API mínima

**Meta:** sustituir placeholders por el modelo real de Akasha.

**Entregables:**

- Tipos base: `KeyPath`, `Value`, `DatasetSource`.
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
- Este hito queda como referencia histórica y fue sustituido por backend nativo de `managed_mapped_file` en 0.5.0.

## 0.4.0 — Carga de múltiples datasets

**Meta:** consolidar carga multi-dataset con claves calificadas y aislamiento por dataset.

**Entregables:**

- API única de carga FlexBuffers: `load(source_id, path, create_if_missing=false)`.
- Namespace explícito por dataset en todas las claves (`source_id.algo...`).
- Rechazo de duplicados por `source_id` (`source_already_loaded`).
- Consulta estricta por dataset en `get`/`has`.
- Tests de claves calificadas y rechazo de duplicados.

**Done cuando:**

- El usuario consulta siempre con dataset explícito (`dataset.key...`).
- Duplicados rechazados con error claro.
- Versión y carga sincronizadas con CMake.

**Estado:** completado.

**Completado:**

- API simplificada: `load(source_id, path, create_if_missing=false)`.
- Validación: source_id único (error si ya existe).
- Claves almacenadas bajo namespace del dataset (`source_id`).
- Consultas `get`/`has` resuelven únicamente dentro del dataset indicado en la clave.
- Creación de archivo vacío si no existe y `create_if_missing=true`.
- Versión desde CMake: macro `AKASHA_VERSION` pasada en compilación.
- Ejemplo demuestra duplicados rechazados, crear archivos, y `nullopt` para claves no calificadas.

**Notas de alcance:**

- Por decisión de alcance, tests se difieren a hitos posteriores.
- La resolución cross-dataset no forma parte de 0.4.0 por diseño (aislamiento por dataset).
## 0.5.0 — Escritura controlada y persistencia

**Meta:** permitir actualizaciones seguras de valores.

**Entregables:**

- API de actualización: `set(key_path, value)`.
- Persistencia directa sobre archivo mapeado en cada `set`.
- Reglas de tipado y validación en escritura.
- Persistencia sobre archivo mapeado con política de crecimiento.
- Redimensionado dinámico del fichero mapeado cuando la capacidad sea insuficiente (grow + remap seguro).
- API de limpieza (`clear`) para borrado total o por prefijo.
- Concurrencia local por hilos para `load/get/set/clear`.
- Tests de round-trip (load -> set -> load).

**Done cuando:**

- Se demuestra round-trip sin pérdida en tipos soportados.
- El crecimiento de fichero mapeado es transparente para el consumidor y mantiene consistencia.
- Las operaciones concurrentes locales (mismo proceso) respetan exclusión y visibilidad.

**Estado:** completado.

**Completado:**

- `set(key_path, value)` implementado con validación de rutas y conflictos.
- Persistencia inmediata en `set` usando `Boost.Interprocess` (`read_write` + `flush`), sin `ofstream` en la librería.
- Redimensionado dinámico (`grow + remap`) con reintentos de escritura.
- `clear(key_path = {})` implementado para borrado total, de dataset y por prefijo.
- Limpieza completa con recreación de fichero al tamaño base configurado.
- Concurrencia local por hilos con bloqueos por fichero (`shared_mutex`).
- Ejemplo actualizado con validaciones de round-trip y escenarios de acceso concurrente local.

## 0.6.0 — Rendimiento y robustez local (MVP)

**Meta:** maximizar rendimiento en proceso único sin complicar la API pública.

**Entregables:**

- Ajustes de hot path en `set/get` para reducir asignaciones y trabajo redundante.
- Política de crecimiento configurable (tamaño inicial, paso de grow, reintentos máximos).
- API de mantenimiento para compactación explícita bajo demanda (sin trabajo en background).
- Helpers template para acceso type-safe: `get<T>()` y `set<T>()`.
- Abstracción de parámetros de rendimiento sin mezclar con datos de usuario.

**Done cuando:**

- Se observa mejora medible de rendimiento en escenarios representativos.
- La compactación y la política de crecimiento son configurables sin romper la API actual.
- Los helpers template ofrecen alternativa type-safe sin casting manual.

**Estado:** completado.

**Completado:**

- `PerformanceTuning` struct con parámetros configurables: initial_size, grow_step, max_retries.
- Método `set_performance_tuning(tuning)` y `performance_tuning()` getter.
- Método `compact(dataset_id={})` para compactación explícita (full si vacío, de dataset si especificado).
- Template `get<T>(key_path)` → `std::optional<T>` con extracción type-safe.
- Template `set<T>(key_path, value)` para escritura type-safe sin casting.
- Ejemplo actualizado: demo de helpers, tests con get<T>/set<T> simplificados.
- Compilación limpia, ejecución funcional sin overhead de estadísticas/métricas.
- **Política:** Sin trackers/stats internos; librería almacena solo datos de usuario.

## 0.7.0 — Modelo de errores y observabilidad

**Meta:** mejorar diagnósticos y soporte de operación.

**Entregables:**

- Jerarquía clara de errores/códigos (`parse_error`, `not_found`, etc.).
- Mensajes de error consistentes para consumidor.

**Done cuando:**

- Los fallos más frecuentes están cubiertos con mensajes accionables.

**Estado:** completado.

**Completado:**

- `Status` enum consolidado: 10 códigos (ok, invalid_key_path, key_conflict, file_read_error, file_write_error, file_not_found, file_full, parse_error, dataset_not_found, source_already_loaded).
- Todos los métodos públicos (`load`, `set<T>`, `get<T>`, `clear`, `compact`, `has`) retornan `Status` (reemplazó LoadStatus/WriteStatus).
- Método `last_status() const noexcept → Status` para consultar el último status de operación.
- Miembro privado `mutable Status last_status_` para registro de errores con update thread-safe.
- Ejemplo actualizado validando retornos de Status y función `last_status()`.
- Compilación limpia, ejecución funcional con tracking automático de errores sin overhead.

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

- Sin issues críticos abiertos en core API, parseo y resolución por dataset.
- Contratos de errores y comportamiento documentados.
- Build e integración verificables desde cero con Conan + CMake.

## Fuera de alcance (antes de 1.0.0)

- Hot reload avanzado.
- Sistema de plugins de backends.
- Replicación distribuida entre nodos.
- Bindings para otros lenguajes.

## Orden recomendado de ejecución inmediata

1. Refinar 0.7.0 (modelo de errores y observabilidad).
2. Reforzar 0.8.0 (empaquetado y DX de integración).
3. Reforzar 0.9.0 (tests de endurecimiento).

Con esos tres hitos, Akasha consolida una propuesta de valor rápida, integrable y lista para estabilización.
