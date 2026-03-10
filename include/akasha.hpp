#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace akasha {

/**
 * @brief Versión semántica de la librería.
 * @return Vista inmutable con la versión actual.
 */
[[nodiscard]] inline std::string_view version() noexcept { return "0.3.0";}

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
 * @brief Vista de lectura de un valor.
 *
 * Es equivalente a Value, pero para cadenas usa std::string_view para
 * evitar copias durante consultas.
 */
using ValueView = std::variant<bool, std::int64_t, std::uint64_t, double, std::string_view>;

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
	struct Node;
	struct MappedFileStorage;

	struct MappedValueRef {
		std::shared_ptr<MappedFileStorage> storage;
		std::vector<std::string> segments;
	};

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
		 * - ValueView si apunta a una hoja.
		 * - DatasetView si apunta a un nodo intermedio.
		 */
		[[nodiscard]] std::optional<std::variant<ValueView, DatasetView>> get(std::string_view key_path) const;

		/**
		 * @brief Obtiene la lista de claves directas disponibles en esta vista.
		 * @return Vector con las claves inmediatas (sin incluir subclaves).
		 */
		[[nodiscard]] std::vector<std::string> keys() const;

	private:
		friend class Store;
		explicit DatasetView(const Node* node) noexcept;
		const Node* node_{nullptr};
	};

	/**
	 * @brief Resultado de una consulta sobre Store.
	 *
	 * Puede contener:
	 * - ValueView (hoja)
	 * - DatasetView (subárbol)
	 */
	using QueryResult = std::variant<ValueView, DatasetView>;

	/**
	 * @brief Carga o actualiza entradas en el almacén.
	 *
	 * Las claves deben ser completas e incluir dataset.
	 * Si hay error de validación o conflicto, no se aplica ningún cambio.
	 *
	 * @param source Colección clave completa -> valor.
	 * @return Estado de la operación.
	 */
	[[nodiscard]] LoadStatus load(const DatasetSource& source);

	/**
	 * @brief Carga configuración desde un archivo FlexBuffers.
	 *
	 * Formato esperado:
	 * - Raíz tipo mapa.
	 * - Hojas de tipo bool, int, uint, double o string.
	 * - Se admiten mapas anidados, que se convierten a KeyPath con dot notation.
	 * - Los valores no se copian: se mantienen referencias al fichero mapeado.
	 *
	 * @param file_path Ruta del archivo FlexBuffers.
	 * @return Estado de la operación de carga/parseo.
	 */
	[[nodiscard]] LoadStatus load_flexbuffer_file(std::string_view file_path);

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
	 * - ValueView si apunta a una hoja.
	 * - DatasetView si apunta a un nodo intermedio.
	 */
	[[nodiscard]] std::optional<QueryResult> get(std::string_view key_path) const;

private:
	struct Node {
		std::optional<Value> value;
		std::optional<MappedValueRef> mapped_value;
		std::unordered_map<std::string, Node> children;
	};

	[[nodiscard]] static bool split_key_path(std::string_view key_path, std::vector<std::string_view>& segments);
	[[nodiscard]] static std::optional<QueryResult> get_from_node(const Node& base, std::string_view key_path);

	Node root_;
};

}  // namespace akasha
