#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace akasha {

/**
 * @brief Versión semántica de la librería.
 * @return Vista inmutable con la versión actual (desde compilación).
 */
[[nodiscard]] inline std::string_view version() noexcept {
#ifdef AKASHA_VERSION
	return AKASHA_VERSION;
#else
	return "0.0.0-dev";  // fallback si no se compila con CMake
#endif
}

/**
 * @brief Ruta jerárquica en notación por puntos.
 *
 * Reglas de formato para claves cargables:
 * - Deben contener al menos 2 segmentos: "dataset.algo".
 * - El primer segmento identifica el dataset.
 * - El resto identifica la clave dentro del dataset.
 *
 * Ejemplos válidos: "core.timeout", "core.settings.retries".
 */
using KeyPath = std::string;

/** @brief Tipo de valor persistido en Akasha. */
using Value = std::variant<bool, std::int64_t, std::uint64_t, double, std::string>;

/**
 * @brief Fuente de carga en memoria.
 *
 * Mapea clave completa -> valor. Ejemplo:
 * - "core.timeout" -> 30
 * - "core.settings.enabled" -> true
 */
using DatasetSource = std::unordered_map<KeyPath, Value>;

/** @brief Resultado de una operación de carga. */
enum class LoadStatus {
	ok,
	invalid_key_path,
	key_conflict,
	file_read_error,
	parse_error,
	unsupported_value_type,
	source_already_loaded,
};

/** @brief Resultado de operaciones de escritura/persistencia. */
enum class WriteStatus {
	ok,
	invalid_key_path,
	key_conflict,
	dataset_not_found,
	file_write_error,
};

struct PerformanceTuning {
	std::size_t initial_mapped_file_size{64 * 1024};
	std::size_t initial_grow_step{(64 * 1024) / 2};
	int max_grow_retries{8};
};

/**
 * @brief Almacén jerárquico de configuración.
 *
 * Store carga rutas completas (incluyendo dataset) y permite:
 * - Consultar si existe una ruta con has.
 * - Obtener un valor hoja o un subconjunto navegable con get.
 */
class Store {
private:
	struct MappedFileStorage;
	struct Source;

public:
	/**
	 * @brief Vista de solo lectura sobre un nodo intermedio del árbol.
	 *
	 * Permite continuar navegando de forma relativa mediante get/has.
	 * Ejemplo: si get("core.settings") devuelve DatasetView, luego
	 * se puede consultar get("enabled") sobre esa vista.
	 */
	class DatasetView {
	public:
		/**
		 * @brief Indica si existe la ruta relativa dentro de esta vista.
		 * @param key_path Ruta relativa a la vista actual.
		 */
		[[nodiscard]] bool has(std::string_view key_path) const;

		/**
		 * @brief Obtiene un valor o sub-vista relativa.
		 * @param key_path Ruta relativa a la vista actual.
		 * @return
		 * - std::nullopt si no existe.
		 * - Value si apunta a una hoja.
		 * - DatasetView si apunta a un nodo intermedio.
		 */
		[[nodiscard]] std::optional<std::variant<Value, DatasetView>> get(std::string_view key_path) const;

		/**
		 * @brief Obtiene la lista de claves directas disponibles en esta vista.
		 * @return Vector con las claves inmediatas (sin incluir subclaves).
		 */
		[[nodiscard]] std::vector<std::string> keys() const;

	private:
		friend class Store;
	
		DatasetView(const Source* source, std::string prefix = {}) noexcept 
			: source_{source}, prefix_{std::move(prefix)} {}
	
		const Source* source_{nullptr};
		std::string prefix_;
	};

	/**
	 * @brief Resultado de una consulta sobre Store.
	 *
	 * Puede contener:
	 * - Value (hoja)
	 * - DatasetView (subárbol)
	 */
	using QueryResult = std::variant<Value, DatasetView>;

	/**
	 * @brief Carga configuración desde un archivo de memoria mapeada.
	 *
	 * Usa Boost.Interprocess managed_mapped_file para almacenar directamente
	 * un map<string, value> en el archivo. Esto permite lecturas zero-copy
	 * y escrituras directas sin reserialización.
	 *
	 * - Si la fuente ya existe (mismo source_id), devuelve error `source_already_loaded`.
	 *
	 * @param source_id Identificador único de la fuente (nombre del dataset).
	 * @param file_path Ruta del archivo de memoria mapeada.
	 * @param create_if_missing Si es true y el archivo no existe, crea uno vacío.
	 * @return Estado de la operación de carga.
	 */
	[[nodiscard]] LoadStatus load(
		std::string_view source_id,
		std::string_view file_path,
		bool create_if_missing = false
	);

	/**
	 * @brief Establece o reemplaza un valor hoja en una clave calificada por dataset.
	 *
	 * Ejemplo: set("user.core.timeout", 90)
	 * La escritura se realiza directamente en el managed_mapped_file (sin reserialización).
	 */
	[[nodiscard]] WriteStatus set(std::string_view key_path, const Value& value);

	/**
	 * @brief Ajusta parámetros de rendimiento local para nuevos crecimientos/creaciones.
	 */
	void set_performance_tuning(const PerformanceTuning& tuning) noexcept;

	/**
	 * @brief Obtiene la configuración de rendimiento actual.
	 */
	[[nodiscard]] PerformanceTuning performance_tuning() const noexcept;

	/**
	 * @brief Elimina datos persistidos.
	 *
	 * - Si key_path está vacío, elimina todos los datos de todos los datasets cargados.
	 * - Si key_path incluye solo dataset (p.ej. "user"), elimina todo ese dataset.
	 * - Si key_path incluye subclave (p.ej. "user.core"), elimina esa clave y todo su subárbol.
	 */
	[[nodiscard]] WriteStatus clear(std::string_view key_path = {});

	/**
	 * @brief Compacta el archivo mapeado para un dataset o para todos.
	 *
	 * - Si dataset_id está vacío, compacta todos los archivos de datasets cargados.
	 * - Si dataset_id existe, compacta solo su archivo asociado.
	 */
	[[nodiscard]] WriteStatus compact(std::string_view dataset_id = {});

	/**
	 * @brief Indica si existe una ruta completa.
	 * @param key_path Ruta completa (incluye dataset).
	 */
	[[nodiscard]] bool has(std::string_view key_path) const;

	/**
	 * @brief Consulta una ruta completa.
	 * @param key_path Ruta completa (incluye dataset).
	 * @return
	 * - std::nullopt si no existe.
	 * - Value si apunta a una hoja.
	 * - DatasetView si apunta a un nodo intermedio.
	 */
	[[nodiscard]] std::optional<QueryResult> get(std::string_view key_path) const;

	/**
	 * @brief Obtiene un valor tipado de forma segura.
	 *
	 * Realiza un `get()` normal y extrae el tipo T esperado.
	 * Si el valor no existe o tiene tipo diferente, devuelve std::nullopt.
	 *
	 * Tipos soportados: bool, int64_t, uint64_t, double, std::string.
	 *
	 * @param key_path Ruta completa de la clave (incluye dataset).
	 * @return std::optional<T> con el valor tipado, o std::nullopt si no existe o tipo no coincide.
	 */
	template<typename T>
	[[nodiscard]] std::optional<T> get(std::string_view key_path) const {
		const auto result = this->Store::get(key_path);
		if (!result.has_value()) {
			return std::nullopt;
		}

		const auto* value = std::get_if<Value>(&*result);
		if (value == nullptr) {
			return std::nullopt;
		}

		const auto* typed_value = std::get_if<T>(value);
		if (typed_value == nullptr) {
			return std::nullopt;
		}

		return *typed_value;
	}

	/**
	 * @brief Establece un valor con un tipo específico.
	 *
	 * Simplifica la creación de Value a partir de un tipo T.
	 * La actualización se registra en las métricas como un `set()` normal.
	 *
	 * Tipos soportados: bool, int64_t, uint64_t, double, std::string, std::string_view.
	 *
	 * @param key_path Ruta completa de la clave (incluye dataset).
	 * @param value Valor tipado a persistir.
	 * @return WriteStatus de la operación.
	 */
	template<typename T>
	[[nodiscard]] WriteStatus set(std::string_view key_path, const T& value) {
		// Construir Value genérico según el tipo
		if constexpr (std::is_same_v<T, std::string_view>) {
			return this->Store::set(key_path, Value{std::string{value}});
		} else if constexpr (std::is_same_v<T, const char*>) {
			return this->Store::set(key_path, Value{std::string{value}});
		} else if constexpr (std::is_same_v<T, bool> ||
							  std::is_same_v<T, std::int64_t> ||
							  std::is_same_v<T, std::uint64_t> ||
							  std::is_same_v<T, double> ||
							  std::is_same_v<T, std::string>) {
			return this->Store::set(key_path, Value{value});
		} else {
			// Tipo no soportado: compilación fallará si se intenta usar
			static_assert(false, "Unsupported type for akasha::Store::set<T>()");
		}
	}

private:
	struct Source {
		std::string id;
		std::string file_path;
		std::shared_ptr<MappedFileStorage> storage;
		std::shared_ptr<std::shared_mutex> file_lock;
		void* dataset_map{nullptr};
	};

	[[nodiscard]] static bool split_key_path(std::string_view key_path, std::vector<std::string_view>& segments);
	[[nodiscard]] Source* find_source(std::string_view source_id);
	[[nodiscard]] const Source* find_source(std::string_view source_id) const;
	[[nodiscard]] std::shared_ptr<std::shared_mutex> get_or_create_file_lock(const std::string& file_path) const;
	[[nodiscard]] bool grow_and_remap_sources_for_path(const std::string& file_path, std::size_t grow_by_bytes);
	[[nodiscard]] bool shrink_and_remap_sources_for_path(const std::string& file_path);
	[[nodiscard]] bool compact_and_remap_sources_for_path(const std::string& file_path);

	std::vector<Source> sources_;
	mutable std::shared_mutex sources_mutex_;
	mutable std::mutex file_locks_mutex_;
	mutable std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> file_locks_;
	std::atomic<std::size_t> initial_mapped_file_size_{64 * 1024};
	std::atomic<std::size_t> initial_grow_step_{(64 * 1024) / 2};
	std::atomic<int> max_grow_retries_{8};
};

}  // namespace akasha
