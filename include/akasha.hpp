#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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

/** @brief Resultado de cualquier operación (carga, escritura, etc). */
enum class Status {
	ok,
	invalid_key_path,      // Clave no tiene al menos 2 segmentos (dataset.clave)
	key_conflict,          // Conflicto en estructura jerárquica
	file_read_error,       // Error al leer archivo mapeado
	file_write_error,      // Error al escribir en archivo
	file_not_found,        // Archivo no existe y create_if_missing=false
	file_full,             // Archivo lleno después de reintentos de crecimiento
	parse_error,           // Error al parsear datos internos
	dataset_not_found,     // Dataset (primer segmento) no existe
	source_already_loaded, // Dataset ya está cargado
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
		 * @brief Obtiene un valor tipado relativo a esta vista.
		 * @param key_path Ruta relativa a la vista actual.
		 * @return std::optional<T> con el valor si existe, std::nullopt en caso contrario.
		 * @note El usuario es responsable del tipo T. Si no coincide con los datos, resultado indefinido.
		 */
		template<typename T = DatasetView>
		[[nodiscard]] std::optional<T> get(std::string_view key_path) const {
			if (source_ == nullptr) {
				return std::nullopt;
			}

			// Construir la clave completa
			std::string full_key = prefix_;
			if (!prefix_.empty() && !key_path.empty()) {
				full_key += '.';
			}
			full_key.append(key_path);

			// Si T es DatasetView, retornar una vista
			if constexpr (std::is_same_v<T, DatasetView>) {
				return DatasetView(source_, full_key);
			} else {
				// Para otros tipos T, acceder al Store backend
				if (source_->store == nullptr) {
					return std::nullopt;
				}
				return source_->store->get<T>(full_key);
			}
		}

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
	[[nodiscard]] Status load(
		std::string_view source_id,
		std::string_view file_path,
		bool create_if_missing = false
	);

	/**
	 * @brief Establece o reemplaza un valor hoja tipado en una clave calificada por dataset.
	 *
	 * Ejemplo: set<int64_t>("user.core.timeout", 90)
	 * La escritura se realiza directamente en el managed_mapped_file como bytes crudos.
	 * El usuario es 100% responsable de que el tipo T sea consistente al leer.
	 */
	template<typename T>
	[[nodiscard]] Status set(std::string_view key_path, const T& value) {
		static_assert(std::is_trivially_copyable_v<T>, 
			"Type T must be trivially copyable for akasha::Store::set<T>");
		
		const char* bytes = reinterpret_cast<const char*>(&value);
		return set_bytes_impl(key_path, bytes, sizeof(T));
	}

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
	[[nodiscard]] Status clear(std::string_view key_path = {});

	/**
	 * @brief Compacta el archivo mapeado para un dataset o para todos.
	 *
	 * - Si dataset_id está vacío, compacta todos los archivos de datasets cargados.
	 * - Si dataset_id existe, compacta solo su archivo asociado.
	 */
	[[nodiscard]] Status compact(std::string_view dataset_id = {});

	/**
	 * @brief Indica si existe una ruta completa.
	 * @param key_path Ruta completa (incluye dataset).
	 */
	[[nodiscard]] bool has(std::string_view key_path) const;

	/**
	 * @brief Obtiene el último status retornado por cualquier operación.
	 * @return Status del último error, o Status::ok si última operación fue exitosa.
	 */
	[[nodiscard]] Status last_status() const noexcept;

	/**
	 * @brief Consulta una ruta completa tipada o retorna DatasetView.
	 * @param key_path Ruta completa (incluye dataset).
	 * @return std::optional<T> con el valor si existe, std::nullopt en caso contrario.
	 * @note El usuario es responsable del tipo T. Si no coincide con los datos, resultado indefinido.
	 */
	template<typename T = DatasetView>
	[[nodiscard]] std::optional<T> get(std::string_view key_path) const {
		// Si T es DatasetView, retornar una vista
		if constexpr (std::is_same_v<T, DatasetView>) {
			return get_dataset_view(key_path);
		} else {
			static_assert(std::is_trivially_copyable_v<T>, 
				"Type T must be trivially copyable for akasha::Store::get<T>");
			
			// Leer bytes genéricos y interpretar como T
			auto bytes = get_bytes_impl(key_path, sizeof(T));
			if (!bytes.has_value()) {
				return std::nullopt;
			}
			
			// Reinterpret bytes como T
			return *reinterpret_cast<const T*>(bytes->data());
		}
	}

	/**
	 * @brief Obtiene un valor tipado o lo establece con un default si no existe.
	 * 
	 * Si la clave existe, retorna su valor. Si no existe, establece el valor por defecto,
	 * lo persiste y retorna ese mismo valor.
	 * 
	 * Útil para inicialización lazy de valores de configuración.
	 * 
	 * @param key_path Ruta completa (incluye dataset).
	 * @param default_value Valor por defecto a establecer si no existe.
	 * @return std::optional<T> con el valor encontrado o el default establecido.
	 */
	template<typename T>
	[[nodiscard]] std::optional<T> getorset(std::string_view key_path, const T& default_value) {
		static_assert(std::is_trivially_copyable_v<T>, 
			"Type T must be trivially copyable for akasha::Store::getorset<T>");
		
		// Intentar obtener el valor existente
		auto existing = get<T>(key_path);
		if (existing.has_value()) {
			return existing;
		}
		
		// Si no existe, escribir el default
		const auto set_status = set<T>(key_path, default_value);
		if (set_status == Status::ok) {
			return default_value;
		}
		
		// Error al escribir
		return std::nullopt;
	}

private:
	struct Source {
		std::string id;
		std::string file_path;
		std::shared_ptr<MappedFileStorage> storage;
		std::shared_ptr<std::shared_mutex> file_lock;
		void* dataset_map{nullptr};
		Store* store{nullptr};
	};

	[[nodiscard]] static bool split_key_path(std::string_view key_path, std::vector<std::string_view>& segments);
	[[nodiscard]] Source* find_source(std::string_view source_id);
	[[nodiscard]] const Source* find_source(std::string_view source_id) const;
	[[nodiscard]] std::optional<DatasetView> get_dataset_view(std::string_view key_path) const;
	[[nodiscard]] Status set_bytes_impl(std::string_view key_path, const void* bytes, std::size_t size);
	[[nodiscard]] std::optional<std::vector<char>> get_bytes_impl(std::string_view key_path, std::size_t expected_size) const;
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
	mutable Status last_status_{Status::ok};
};

// Template specializations for std::string outside the class scope

/**
 * @brief Especialización para Store::set() con std::string.
 * Serializa la string como [size_t length][char data].
 */
template<>
inline akasha::Status Store::set<std::string>(std::string_view key_path, const std::string& value) {
	std::vector<char> buffer;
	std::size_t len = value.size();
	
	const char* len_bytes = reinterpret_cast<const char*>(&len);
	buffer.insert(buffer.end(), len_bytes, len_bytes + sizeof(std::size_t));
	buffer.insert(buffer.end(), value.begin(), value.end());
	
	return set_bytes_impl(key_path, buffer.data(), buffer.size());
}

/**
 * @brief Especialización para Store::get() con std::string.
 * Deserializa la string desde [size_t length][char data].
 */
template<>
inline std::optional<std::string> Store::get<std::string>(std::string_view key_path) const {
	// Necesitamos al menos sizeof(size_t) para la longitud
	auto bytes = get_bytes_impl(key_path, sizeof(std::size_t));
	if (!bytes.has_value() || bytes->size() < sizeof(std::size_t)) {
		return std::nullopt;
	}
	
	// Leer la longitud
	std::size_t len = *reinterpret_cast<const std::size_t*>(bytes->data());
	
	// Verificar que el tamaño es consistente
	if (bytes->size() != sizeof(std::size_t) + len) {
		return std::nullopt;
	}
	
	// Reconstruir la string a partir de los bytes
	if (len == 0) {
		return std::string();
	}
	
	return std::string(bytes->data() + sizeof(std::size_t), bytes->data() + bytes->size());
}

/**
 * @brief Especialización para Store::getorset() con std::string.
 * Obtiene la string o establece el default si no existe.
 */
template<>
inline std::optional<std::string> Store::getorset<std::string>(std::string_view key_path, const std::string& default_value) {
	// Intentar obtener el valor existente
	auto existing = get<std::string>(key_path);
	if (existing.has_value()) {
		return existing;
	}
	
	// Si no existe, escribir el default
	const auto set_status = set<std::string>(key_path, default_value);
	if (set_status == Status::ok) {
		return default_value;
	}
	
	// Error al escribir
	return std::nullopt;
}

}  // namespace akasha
